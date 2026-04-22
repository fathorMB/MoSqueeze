# Worker Spec: Disable Memory Tracking in Parallel Benchmark Mode

**Issue:** #38  
**Branch:** `feat/benchmark-improvements`  
**Priority:** Medium  
**Type:** Bug fix

---

## Problem Statement

Memory tracking in parallel benchmarks reports meaningless values (~5.4 GB per file) because `getPeakMemoryUsage()` measures the **entire process working set**, not per-operation memory.

### Root Cause

```cpp
// src/libmosqueeze/src/platform/Platform.cpp
size_t getPeakMemoryUsage() {
#if defined(MOSQUEEZE_WINDOWS)
    PROCESS_MEMORY_COUNTERS counters{};
    GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters));
    return counters.PeakWorkingSetSize;  // ← Process-wide, not per-thread
#endif
}
```

With 8 threads running concurrently:
- All engine libraries (Zstd, LZMA, Brotli, ZPAQ) are loaded
- Each thread allocates buffers
- Working set accumulates across all threads
- Final measurement shows total process memory, not per-file usage

### Evidence from Benchmark

| Algorithm | Avg Memory Reported | Avg File Size | Reality |
|-----------|---------------------|---------------|---------|
| zstd | 5,389 MB | 177 KB | Wrong — should be ~1-5 MB |
| xz | 5,389 MB | 177 KB | Wrong |
| brotli | 5,389 MB | 177 KB | Wrong |
| zpaq | 5,389 MB | 177 KB | Wrong |

All show identical memory because it's the cumulative process working set.

---

## Implementation Plan

### Step 1: Disable memory tracking in `runParallel()`

**File:** `src/mosqueeze-bench/src/BenchmarkRunner.cpp`

**Location:** `runParallel()` function (around line 336)

**Current code:**
```cpp
std::vector<BenchmarkResult> BenchmarkRunner::runParallel(const BenchmarkConfig& config) {
    // ... existing code ...
    // Uses config.trackMemory directly
}
```

**Change:**
```cpp
std::vector<BenchmarkResult> BenchmarkRunner::runParallel(const BenchmarkConfig& config) {
    // Create a modified config with memory tracking disabled
    BenchmarkConfig parallelConfig = config;
    parallelConfig.trackMemory = false;
    
    // Use parallelConfig instead of config for all operations
    // ... rest of the function ...
}
```

### Step 2: Update `BenchmarkRunner::runWithConfig()` for sequential mode

**File:** `src/mosqueeze-bench/src/BenchmarkRunner.cpp`

**Location:** `runWithConfig()` function (around line 193)

**Current:** Uses `config.trackMemory` directly — this is **correct** for sequential mode.

**No changes needed** — sequential mode already works correctly because there's no cross-thread memory accumulation.

### Step 3: Add documentation comment

**File:** `src/mosqueeze-bench/include/mosqueeze/bench/BenchmarkConfig.hpp`

Add documentation:
```cpp
/**
 * @brief Configuration for benchmark runs
 */
struct BenchmarkConfig {
    // ... existing fields ...
    
    /**
     * @brief Track peak memory usage during compression
     * @note In parallel mode (>1 thread), this is automatically disabled
     *       because process-wide memory tracking cannot attribute memory
     *       to individual operations. Use sequential mode for accurate
     *       memory measurements.
     */
    bool trackMemory = true;
};
```

---

## Files to Modify

| File | Change |
|------|--------|
| `src/mosqueeze-bench/src/BenchmarkRunner.cpp` | Disable `trackMemory` in `runParallel()` |
| `src/mosqueeze-bench/include/mosqueeze/bench/BenchmarkConfig.hpp` | Add documentation comment |

---

## Testing Plan

### Test 1: Parallel mode shows zero memory

```bash
mosqueeze-bench -c "./images" --default-only --threads 8 -v -o "./results"
# Verify: peak_memory_bytes column should be 0 for all results
```

### Test 2: Sequential mode still reports memory

```bash
mosqueeze-bench -c "./images" --default-only --sequential -v -o "./results"
# Verify: peak_memory_bytes should show reasonable values (not 5GB)
```

### Test 3: Single-threaded (default) reports memory

```bash
mosqueeze-bench -c "./images" --default-only -v -o "./results"
# Verify: peak_memory_bytes shows increasing values as files are processed
```

### Test 4: SQLite results

```sql
-- Check all parallel results have 0 memory
SELECT DISTINCT peak_memory_bytes FROM results WHERE peak_memory_bytes > 0;
-- Should return nothing for parallel run

-- Check sequential results have reasonable values
SELECT AVG(peak_memory_bytes), MAX(peak_memory_bytes) FROM results;
-- Should show MBs, not GBs
```

---

## Verification Checklist

- [ ] Parallel mode (`--threads N`) sets memory to 0 in results
- [ ] Sequential mode (`--sequential`) reports accurate memory
- [ ] Default mode (single thread) reports accurate memory
- [ ] SQLite export reflects correct values
- [ ] JSON export reflects correct values
- [ ] CSV export reflects correct values
- [ ] Console output shows 0 MB or N/A for parallel mode

---

## Alternative Solutions Considered

### Option A: Per-thread memory tracking
**Rejected:** Windows has no per-thread memory API. Linux has `RUSAGE_THREAD` but it's not portable.

### Option B: Custom allocator
**Rejected:** Overriding `new`/`delete` is invasive and can hide real memory usage from libraries.

### Option C: Memory budget estimation
**Rejected:** Theoretical estimates are inaccurate and misleading.

**Chosen solution:** Simply disable memory tracking in parallel mode. Users who need memory data should run in sequential mode.

---

## Estimated Effort

| Task | Time |
|------|------|
| Code changes | 15 min |
| Testing | 30 min |
| Documentation | 10 min |
| **Total** | **55 min** |

---

## Dependencies

- None — standalone fix

---

## Related Issues

- Issue #33 — ThreadPool deadlock fix (parallel mode now works)
- Original benchmark that revealed the issue