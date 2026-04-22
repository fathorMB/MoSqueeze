# Worker Spec: Fix ThreadPool Deadlock in Parallel Benchmark

**Issue:** #33  
**Branch:** `feat/33-threadpool-deadlock`  
**Priority:** High — blocks parallel benchmark feature

---

## Problem Statement

The parallel benchmark implementation (`--threads N`) hangs indefinitely while the sequential mode works correctly.

### Symptoms

| Command | Behavior |
|---------|----------|
| `mosqueeze-bench -c ./images --default-only -v` | Works (sequential, shows progress) |
| `mosqueeze-bench -c ./images --default-only --threads 8 -v` | Prints "Running with 8 threads..." then hangs |
| `mosqueeze-bench -c ./images --default-only --sequential -v` | No progress output |
| `mosqueeze-bench -f single.jpg --default-only -v` | Shows wrong thread count (16 instead of 1) |

### Root Cause Analysis

After code review, the issue is in **`ThreadPool::waitAll()` (line 56-61)**:

```cpp
void ThreadPool::waitAll() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    completed_.wait(lock, [this] {
        return tasks_.empty() && activeTasks_ == 0;
    });
}
```

**The race condition:**
1. Worker thread completes a task and calls `--activeTasks_` followed by `completed_.notify_all()`
2. If `waitAll()` thread is not actively waiting on the condition variable when `notify_all()` is called, the notification is lost
3. `waitAll()` then waits forever because the predicate is already true, but the condition variable has no pending notification

**Additional issues found:**
- Progress callback in `runParallel()` lacks `algorithm` and `level` info (incomplete `ProgressInfo`)
- Thread count detection returns wrong value for single-file mode

---

## Implementation Plan

### Step 1: Fix ThreadPool::waitAll() race condition

**File:** `src/mosqueeze-bench/src/ThreadPool.cpp`

**Change:** Use `notify_one` at task submission and ensure predicate check happens under lock.

```cpp
void ThreadPool::waitAll() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    completed_.wait(lock, [this] {
        return tasks_.empty() && activeTasks_.load() == 0;
    });
}
```

Actually, the better fix is to ensure the predicate is checked WITH the notify:

```cpp
// In submit()
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (stop_) {
        throw std::runtime_error("submit on stopped ThreadPool");
    }
    tasks_.emplace([task]() { (*task)(); });
    ++pendingTasks_;  // Track submitted tasks
}
condition_.notify_one();

// In worker thread after task completion
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    --pendingTasks_;
}
completed_.notify_all();

// In waitAll()
void ThreadPool::waitAll() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    completed_.wait(lock, [this] {
        return pendingTasks_.load() == 0;
    });
}
```

**Alternative simpler fix:** Just check `activeTasks_` with proper synchronization:

```cpp
void ThreadPool::waitAll() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    completed_.wait(lock, [this] {
        const bool done = tasks_.empty() && activeTasks_.load() == 0;
        return done;
    });
}
```

But this still has the lost-wakeup issue. The proper fix is to track all submitted tasks:

**Recommended Fix:** Add atomic counter for total submitted tasks:

```cpp
// ThreadPool.hpp - add member
std::atomic<size_t> pendingTasks_{0};

// ThreadPool.cpp - in submit()
++pendingTasks_;

// ThreadPool.cpp - in worker after task()
--pendingTasks_;
completed_.notify_all();

// ThreadPool.cpp - waitAll()
void ThreadPool::waitAll() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    completed_.wait(lock, [this] {
        return pendingTasks_.load() == 0;
    });
}
```

### Step 2: Fix progress callback in runParallel()

**File:** `src/mosqueeze-bench/src/BenchmarkRunner.cpp` (line 362-381)

**Current code:**
```cpp
auto maybeReportProgress = [&](const std::filesystem::path& file, bool force) {
    // ...
    ProgressInfo info{};
    info.currentFile = file;  // Only file is set
    // currentAlgorithm and currentLevel are NOT set
};
```

**Issue:** `ProgressInfo` lacks algorithm/level info in parallel mode, making `-v` output incomplete.

**Fix:** Use atomic trackers for current algorithm/level, or simplify progress to file-only in parallel mode.

Actually, looking at the sequential progress callback (line 238-248), it DOES include algorithm and level. The parallel version doesn't have access to these per-file.

**Recommendation:** Keep progress as file-only in parallel mode (since algorithm/level vary per file), but ensure `completedFiles` and `progress` are correctly calculated.

### Step 3: Fix thread count in single-file mode

**File:** `src/mosqueeze-bench/src/main.cpp` (line 445-452)

**Issue:** Thread count logic uses `config.getEffectiveThreadCount()` but doesn't account for single-file mode properly.

```cpp
if (config.getEffectiveThreadCount() > 1) {
    // parallel mode
}
```

When user specifies `--threads 8` with a single file, it still enters parallel mode but only one task is submitted.

**Fix:** Add check for file count:

```cpp
const bool useParallel = config.getEffectiveThreadCount() > 1 && config.files.size() > 1;
if (useParallel) {
    // parallel mode
} else {
    // sequential mode
}
```

---

## Files to Modify

1. **`src/mosqueeze-bench/src/ThreadPool.cpp`** — Fix waitAll() race condition
2. **`src/mosqueeze-bench/include/mosqueeze/bench/ThreadPool.hpp`** — Add pendingTasks_ counter
3. **`src/mosqueeze-bench/src/main.cpp`** — Fix thread count logic for single-file mode

---

## Testing Plan

### Unit Tests

Add tests to `tests/unit/ThreadPool_test.cpp` (create if needed):

```cpp
TEST(ThreadPool, WaitAllReturnsWhenNoTasks) {
    ThreadPool pool(4);
    pool.waitAll();  // Should return immediately
}

TEST(ThreadPool, WaitAllReturnsAfterTasksComplete) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    
    for (int i = 0; i < 100; ++i) {
        pool.submit([&counter] { ++counter; });
    }
    
    pool.waitAll();
    EXPECT_EQ(counter.load(), 100);
}

TEST(ThreadPool, WaitAllDeadlockRegression) {
    // Submit tasks, then immediately call waitAll
    // This used to deadlock if notify happened before wait
    ThreadPool pool(8);
    std::atomic<int> counter{0};
    
    for (int i = 0; i < 50; ++i) {
        pool.submit([&counter] { ++counter; std::this_thread::sleep_for(std::chrono::milliseconds(1)); });
    }
    pool.waitAll();
    EXPECT_EQ(counter.load(), 50);
}
```

### Integration Tests

Run benchmark with various thread configurations:

```bash
# Single file, should use 1 thread
mosqueeze-bench -f "test.jpg" --default-only -v

# Small corpus with threads
mosqueeze-bench -c "./images" --default-only --threads 1 -v
mosqueeze-bench -c "./images" --default-only --threads 4 -v
mosqueeze-bench -c "./images" --default-only --threads 8 -v

# Verify progress output in both modes
mosqueeze-bench -c "./images" --default-only -v           # Sequential, should show progress
mosqueeze-bench -c "./images" --default-only --threads 4 -v  # Parallel, should show progress
```

---

## Verification Checklist

- [ ] `ThreadPool::waitAll()` returns immediately when no tasks pending
- [ ] `ThreadPool::waitAll()` returns after all submitted tasks complete
- [ ] No deadlock when `waitAll()` is called immediately after `submit()`
- [ ] Progress output works in sequential mode (`-v`)
- [ ] Progress output works in parallel mode (`--threads N -v`)
- [ ] Single file with `--threads N` doesn't hang
- [ ] All existing tests pass
- [ ] New unit tests pass

---

## Estimated Effort

| Task | Time |
|------|------|
| ThreadPool fix | 30 min |
| Add unit tests | 30 min |
| Integration testing | 30 min |
| Code review | 15 min |
| **Total** | **1h 45min** |

---

## Dependencies

- None — this is a standalone bug fix

---

## Risks

1. **Thread sanitizer:** The fix requires careful synchronization. Consider running tests with TSAN enabled.
2. **Windows-specific behavior:** The race may manifest differently on Windows vs Linux. Test on both if possible.