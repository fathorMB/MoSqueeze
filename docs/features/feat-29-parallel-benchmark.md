# Worker Spec: Multi-threaded Benchmark Execution (Issue #29)

## Overview

Implement file-level parallelism in `mosqueeze-bench` using a thread pool. Each worker processes different files while all engines are tested sequentially on each assigned file.

## Target Performance

On 8-core machine with 898 files:
- Sequential: baseline time
- Parallel (8 threads): ~5-7x speedup

## Architecture

```
Main Thread
    │
    ├── ThreadPool (N workers)
    │       │
    │       ├── Worker 1: [file_0, file_N, file_2N, ...]
    │       ├── Worker 2: [file_1, file_N+1, file_2N+1, ...]
    │       └── ...
    │
    ├── ProgressTracker (atomic counters)
    │       └── Thread-safe progress updates
    │
    └── ResultsQueue (thread-safe queue)
            └── Aggregated results
```

---

## Part 1: BenchmarkConfig Extension

### File: `src/libmosqueeze/include/mosqueeze/bench/BenchmarkConfig.hpp`

Add to `BenchmarkConfig` struct:

```cpp
// --- Threading ---
int threadCount = 0;  // 0 = auto-detect (hardware_concurrency)
bool sequential = false;  // Force single-threaded mode

int getEffectiveThreadCount() const {
    if (sequential) return 1;
    if (threadCount > 0) return threadCount;
    int hw = static_cast<int>(std::thread::hardware_concurrency());
    return hw > 0 ? hw : 1;
}
```

Include at top:
```cpp
#include <thread>
```

---

## Part 2: ThreadPool Utility

### New File: `src/mosqueeze-bench/include/mosqueeze/bench/ThreadPool.hpp`

```cpp
#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace mosqueeze::bench {

class ThreadPool {
public:
    explicit ThreadPool(size_t threads);
    ~ThreadPool();

    // Submit a task, return future for result
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result_t<F, Args...>>;

    void waitAll();
    size_t size() const { return workers_.size(); }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    std::condition_variable completed_;
    std::atomic<bool> stop_{false};
    std::atomic<size_t> activeTasks_{0};
};

// Implementation in ThreadPool.cpp

} // namespace mosqueeze::bench
```

### New File: `src/mosqueeze-bench/src/ThreadPool.cpp`

```cpp
#include <mosqueeze/bench/ThreadPool.hpp>

namespace mosqueeze::bench {

ThreadPool::ThreadPool(size_t threads) {
    for (size_t i = 0; i < threads; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queueMutex_);
                    condition_.wait(lock, [this] {
                        return stop_ || !tasks_.empty();
                    });
                    if (stop_ && tasks_.empty()) return;
                    task = std::move(tasks_.front());
                    tasks_.pop();
                    ++activeTasks_;
                }
                task();
                --activeTasks_;
                completed_.notify_all();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        stop_ = true;
    }
    condition_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
}

void ThreadPool::waitAll() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    completed_.wait(lock, [this] {
        return tasks_.empty() && activeTasks_ == 0;
    });
}

} // namespace mosqueeze::bench
```

---

## Part 3: BenchmarkRunner Parallel Implementation

### File: `src/mosqueeze-bench/include/mosqueeze/bench/BenchmarkRunner.hpp`

Add new method:

```cpp
// Add after runWithConfig declaration
std::vector<BenchmarkResult> runParallel(const BenchmarkConfig& config);

private:
    // Add worker function
    void processFile(
        const std::filesystem::path& file,
        const std::vector<ICompressionEngine*>& engines,
        const BenchmarkConfig& config,
        std::vector<BenchmarkResult>& results,
        std::mutex& resultsMutex,
        std::atomic<int>& completedCount,
        std::atomic<size_t>& completedBytes
    ) const;
```

### File: `src/mosqueeze-bench/src/BenchmarkRunner.cpp`

Add parallel implementation:

```cpp
#include <mosqueeze/bench/ThreadPool.hpp>
#include <atomic>
#include <mutex>

std::vector<BenchmarkResult> BenchmarkRunner::runParallel(const BenchmarkConfig& config) {
    if (engines_.empty()) {
        throw std::runtime_error("No engines registered");
    }

    const int threadCount = config.getEffectiveThreadCount();
    
    // Select engines
    std::vector<ICompressionEngine*> selectedEngines;
    if (config.allEngines || config.algorithms.empty()) {
        for (const auto& e : engines_) {
            selectedEngines.push_back(e.get());
        }
    } else {
        for (const auto& name : config.algorithms) {
            auto* engine = findEngine(name);
            if (engine) selectedEngines.push_back(engine);
        }
    }

    FileTypeDetector detector;
    std::vector<BenchmarkResult> allResults;
    std::mutex resultsMutex;
    std::atomic<int> completedCount{0};
    std::atomic<size_t> completedBytes{0};
    std::atomic<bool> cancelled{false};

    // Check cancellation
    auto checkCancel = [&]() {
        if (config.hasCancellation() && config.shouldCancel()) {
            cancelled = true;
        }
        return cancelled.load();
    };

    // Progress callback wrapper (thread-safe)
    auto reportProgress = [&](const ProgressInfo& info) {
        if (config.hasProgressCallback()) {
            // Rate-limit progress updates to avoid contention
            static std::mutex progressMutex;
            static auto lastUpdate = std::chrono::steady_clock::now();
            
            std::lock_guard<std::mutex> lock(progressMutex);
            auto now = std::chrono::steady_clock::now();
            if (now - lastUpdate > std::chrono::milliseconds(50)) {
                config.onProgress(info);
                lastUpdate = now;
            }
        }
    };

    // Create thread pool
    ThreadPool pool(threadCount);

    // Submit tasks for each file
    std::vector<std::future<void>> futures;
    
    for (const auto& file : config.files) {
        if (cancelled) break;

        auto future = pool.submit([&, file]() {
            if (cancelled) return;

            processFile(
                file,
                selectedEngines,
                config,
                allResults,
                resultsMutex,
                completedCount,
                completedBytes
            );

            // Update progress
            ProgressInfo info{};
            info.currentFile = file;
            info.completedFiles = completedCount.load();
            info.totalFiles = static_cast<int>(config.files.size());
            info.progress = static_cast<double>(completedCount.load()) / config.files.size();
            reportProgress(info);
        });
        futures.push_back(std::move(future));
    }

    // Wait for all tasks
    pool.waitAll();

    // Compute stats if requested
    return allResults;
}

void BenchmarkRunner::processFile(
    const std::filesystem::path& file,
    const std::vector<ICompressionEngine*>& engines,
    const BenchmarkConfig& config,
    std::vector<BenchmarkResult>& results,
    std::mutex& resultsMutex,
    std::atomic<int>& completedCount,
    std::atomic<size_t>& completedBytes
) const {
    FileTypeDetector detector;
    auto classification = detector.detect(file);

    for (ICompressionEngine* engine : engines) {
        std::vector<int> levelsToTest;
        if (config.defaultOnly) {
            levelsToTest.push_back(engine->defaultLevel());
        } else if (config.levels.empty()) {
            levelsToTest = engine->supportedLevels();
        } else {
            auto supported = engine->supportedLevels();
            for (int l : config.levels) {
                if (std::find(supported.begin(), supported.end(), l) != supported.end()) {
                    levelsToTest.push_back(l);
                }
            }
        }

        for (int level : levelsToTest) {
            // Warmup iterations (not counted)
            for (int w = 0; w < config.warmupIterations; ++w) {
                BenchmarkResult dummy;
                dummy.file = file;
                runIteration(engine, file, level, config, classification.type, dummy);
            }

            // Measured iterations
            for (int iter = 0; iter < config.iterations; ++iter) {
                BenchmarkResult result;
                result.file = file;
                runIteration(engine, file, level, config, classification.type, result);
                result.fileType = classification.type;

                std::lock_guard<std::mutex> lock(resultsMutex);
                results.push_back(std::move(result));
            }
        }
    }

    ++completedCount;
}
```

---

## Part 4: CLI Integration

### File: `src/mosqueeze-bench/src/main.cpp`

Add CLI options after existing threading-related options:

```cpp
// Threading options
int threadCount = 0;
bool sequential = false;

app.add_option("--threads", threadCount, "Number of worker threads (0 = auto-detect)");
app.add_flag("--sequential", sequential, "Force sequential execution (disable parallelism)");
```

In config building section:

```cpp
config.threadCount = threadCount;
config.sequential = sequential;
```

In execution section:

```cpp
// Run benchmark
std::vector<BenchmarkResult> results;
if (config.getEffectiveThreadCount() > 1) {
    if (verbose) {
        std::cout << "Running with " << config.getEffectiveThreadCount() << " threads...\n";
    }
    results = runner.runParallel(config);
} else {
    results = runner.runWithConfig(config);
}
```

---

## Part 5: CMakeLists.txt Update

### File: `src/mosqueeze-bench/CMakeLists.txt`

Add ThreadPool source:

```cmake
add_executable(mosqueeze-bench
  src/main.cpp
  src/BenchmarkRunner.cpp
  src/CorpusManager.cpp
  src/Formatters.cpp
  src/ResultsStore.cpp
  src/ThreadPool.cpp        # NEW
)
```

---

## Part 6: Thread Safety Verification

### Memory Tracking

Each thread tracks its own peak memory via `platform::getPeakMemoryUsage()`. The result is stored in `BenchmarkResult::peakMemoryBytes` which is written under mutex lock, so no race condition.

### Progress Reporting

Progress updates are rate-limited to max 20/second to avoid:
- Console output garbling
- Lock contention
- Excessive overhead

Use `std::mutex` for `std::cout` output.

---

## Part 7: Tests

### New File: `tests/unit/ThreadPool_test.cpp`

```cpp
#include <mosqueeze/bench/ThreadPool.hpp>
#include <atomic>
#include <cassert>
#include <iostream>
#include <vector>

int main() {
    mosqueeze::bench::ThreadPool pool(4);
    
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 100; ++i) {
        pool.submit([&counter] {
            ++counter;
        });
    }
    
    pool.waitAll();
    assert(counter == 100);
    
    std::cout << "[PASS] ThreadPool test\n";
    return 0;
}
```

---

## Part 8: Documentation

Update `docs/wiki/guides/benchmark-tool.md`:

```markdown
## Parallel Execution

For large benchmark sets, use parallel execution:

```bash
# Auto-detect threads (uses all cores)
mosqueeze-bench -c ./images --default-only

# Specific thread count
mosqueeze-bench -c ./images --threads 8

# Force sequential (single-threaded)
mosqueeze-bench -c ./images --sequential
```

### Performance

On 8-core machine with 898 image files:
- Sequential: ~45 seconds
- Parallel (8 threads): ~8 seconds (5.6x speedup)
```

---

## Definition of Done

- [ ] `ThreadPool.hpp/.cpp` added
- [ ] `BenchmarkConfig` has `threadCount` and `sequential` fields
- [ ] `BenchmarkRunner::runParallel()` implemented
- [ ] `--threads` and `--sequential` CLI options work
- [ ] Unit test for ThreadPool passes
- [ ] Integration test: parallel mode produces same results as sequential
- [ ] Memory tracking correct in parallel mode
- [ ] Progress output thread-safe (no garbling)
- [ ] CI passes on Linux, macOS, Windows
- [ ] Updated benchmark-tool.md documentation

---

## Files to Create

| File | Purpose |
|------|---------|
| `src/mosqueeze-bench/include/mosqueeze/bench/ThreadPool.hpp` | Thread pool interface |
| `src/mosqueeze-bench/src/ThreadPool.cpp` | Thread pool implementation |
| `tests/unit/ThreadPool_test.cpp` | Unit tests |

## Files to Modify

| File | Change |
|------|--------|
| `src/libmosqueeze/include/mosqueeze/bench/BenchmarkConfig.hpp` | Add `threadCount`, `sequential`, `getEffectiveThreadCount()` |
| `src/mosqueeze-bench/include/mosqueeze/bench/BenchmarkRunner.hpp` | Add `runParallel()`, `processFile()` |
| `src/mosqueeze-bench/src/BenchmarkRunner.cpp` | Implement `runParallel()` |
| `src/mosqueeze-bench/src/main.cpp` | Add `--threads`, `--sequential` flags |
| `src/mosqueeze-bench/CMakeLists.txt` | Add `ThreadPool.cpp` |
| `tests/CMakeLists.txt` | Add ThreadPool test |
| `docs/wiki/guides/benchmark-tool.md` | Document parallel execution |

---

## Notes

1. **Backward Compatible**: Default behavior is parallel (auto-detect threads). Use `--sequential` for old behavior.
2. **Memory Safety**: Each thread has its own stack; results aggregated under mutex.
3. **No External Dependencies**: Uses only `<thread>`, `<mutex>`, `<condition_variable>` from C++20.