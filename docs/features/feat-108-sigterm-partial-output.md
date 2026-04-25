# Worker Spec: SIGTERM/SIGINT Partial Output

**Issue**: [#108](https://github.com/fathorMB/MoSqueeze/issues/108)  
**Type**: Bug Fix + Enhancement  
**Priority**: P2-Medium  
**Severity**: Major  
**Estimated Effort**: 3-4 hours

---

## Summary

When `mosqueeze-bench` is interrupted by `timeout` (SIGTERM) or Ctrl+C (SIGINT), no partial output is saved. Hours of computation can be lost. This fix adds signal handlers to flush partial results before exit.

---

## Problem Statement

### Current Behavior

```bash
# Long-running benchmark
timeout 300 mosqueeze-bench -d /large-corpus -a all -o /tmp/out

# After 5 minutes: exit code 124 (timeout)
ls -la /tmp/out/
# results.json       <- MISSING
# results.sqlite3     <- INCOMPLETE (no COMMIT)
```

### Impact

| Scenario | Time Lost | Recovery |
|----------|-----------|----------|
| CI timeout | 5-10 minutes | Re-run from scratch |
| Accidental Ctrl+C | Hours (large corpus) | Re-run from scratch |
| System shutdown | Variable | Re-run from scratch |

### Root Cause

1. **No signal handler** — Process terminates immediately on SIGTERM/SIGINT
2. **SQLite transaction** — Results stored in memory, only flushed at completion
3. **JSON/CSV export** — Written only at end of `main()`

---

## Solution

### Design Decision

Implement signal handler that:
1. Sets atomic flag to signal interruption
2. Main loop checks flag and exits gracefully
3. Partial results are flushed to SQLite
4. JSON/CSV exported with `interrupted: true` flag

### Implementation

#### 1. Signal Handler Infrastructure

```cpp
// In main.cpp, before main()
#include <csignal>
#include <atomic>

namespace {
std::atomic<bool> g_interrupted{false};

void signalHandler(int signal) {
    g_interrupted.store(true, std::memory_order_release);
}

void installSignalHandlers() {
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGINT, signalHandler);
}
}
```

#### 2. BenchmarkRunner Interrupt Support

```cpp
// In BenchmarkRunner.hpp
class BenchmarkRunner {
public:
    // Add overload with cancellation support
    std::vector<BenchmarkResult> runWithConfig(
        const BenchmarkConfig& config,
        std::atomic<bool>& interrupted  // New parameter
    );
};
```

```cpp
// In BenchmarkRunner.cpp, inside the benchmark loop
for (const auto& file : config.files) {
    // Check for interruption
    if (interrupted.load(std::memory_order_acquire)) {
        std::cerr << "\n[INTERRUPTED] Received signal, flushing partial results...\n";
        break;
    }
    
    // ... existing benchmark code ...
}
```

#### 3. Partial Output Flag

```cpp
// In main.cpp, after benchmark loop
std::vector<BenchmarkResult> results = runner.runWithConfig(config, g_interrupted);

// Check if interrupted
const bool wasInterrupted = g_interrupted.load(std::memory_order_acquire);

// Always flush results
ResultsStore store(outputPath / "results.sqlite3");
store.saveAll(results);

// Export JSON with interrupt flag
nlohmann::json payload = {
    {"interrupted", wasInterrupted},
    {"completed_files", results.size()},
    {"total_files", config.files.size()},
    {"results", results}  // Existing result serialization
};
std::ofstream(outPath / "results.json") << payload.dump(2);

// Export CSV
store.exportCsv(outPath / "results.csv");

if (wasInterrupted) {
    std::cerr << "[INFO] Partial results saved to " << outputPath << "\n";
    std::cerr << "[INFO] Completed " << results.size() << " of " << config.files.size() << " files\n";
    return 130;  // Standard exit code for SIGINT
}
```

#### 4. Resume Capability (Future Enhancement)

```cpp
// In future: add --resume flag
mosqueeze-bench --resume /tmp/out/results.sqlite3 -d /corpus -a all
```

For now, just store enough state for manual resume via `loadExistingKeys()`.

---

## Files to Modify

| File | Changes |
|------|---------|
| `src/mosqueeze-bench/src/main.cpp` | Add signal handler, interrupt flag, partial output logic |
| `src/mosqueeze-bench/include/mosqueeze/bench/BenchmarkRunner.hpp` | Add interrupt parameter to `runWithConfig()` |
| `src/mosqueeze-bench/src/BenchmarkRunner.cpp` | Check interrupt flag in benchmark loops |
| `src/mosqueeze-bench/include/mosqueeze/bench/ResultsStore.hpp` | Add `interrupted` flag to export functions (optional) |
| `src/mosqueeze-bench/src/ResultsStore.cpp` | Support partial output metadata |

---

## Acceptance Criteria

### Must Have

1. **SIGTERM handling**
   ```bash
   # Start long benchmark with timeout
   timeout 5 mosqueeze-bench -d /tmp/corpus -a all -o /tmp/out &
   
   # After timeout, partial output exists
   ls /tmp/out/results.json
   # Expected: file exists with {"interrupted": true, ...}
   ```

2. **SIGINT handling (Ctrl+C)**
   ```bash
   # Start benchmark
   mosqueeze-bench -d /tmp/corpus -a all -o /tmp/out
   
   # Press Ctrl+C after a few seconds
   ^C
   
   # Verify output
   cat /tmp/out/results.json | jq '.interrupted'
   # Expected: true
   ```

3. **SQLite has partial results**
   ```bash
   timeout 5 mosqueeze-bench -d /tmp/corpus -a zstd -l 1,2,3 -o /tmp/out
   
   # Check SQLite has results
   sqlite3 /tmp/out/results.sqlite3 "SELECT COUNT(*) FROM benchmark_results;"
   # Expected: > 0 (some results before timeout)
   ```

### Should Have

4. **Clear console messages**
   ```
   [INTERRUPTED] Received signal, flushing partial results...
   [INFO] Partial results saved to /tmp/out
   [INFO] Completed 15 of 100 files
   ```

5. **Correct exit code**
   - SIGINT (Ctrl+C) → exit 130
   - SIGTERM (timeout) → exit 143

### Nice to Have

6. **--checkpoint-every N**
   ```bash
   # Auto-save every 10 files
   mosqueeze-bench --checkpoint-every 10 -d /tmp/corpus -o /tmp/out
   ```

7. **--resume**
   ```bash
   # Resume from interrupted run
   mosqueeze-bench --resume /tmp/out/results.sqlite3 -d /tmp/corpus
   ```

---

## Testing Plan

### Manual Testing

```bash
# Setup: create test corpus
mkdir -p /tmp/corpus
for i in {1..50}; do
    dd if=/dev/urandom of=/tmp/corpus/file_$i.bin bs=1M count=5 2>/dev/null
done

# Test 1: SIGTERM via timeout
timeout 10 mosqueeze-bench -d /tmp/corpus -a zstd -l 1,3,5,7,9,11,15,19,22 -o /tmp/out
echo "Exit code: $?"
# Verify: exit code 143 or 124, partial results exist

# Test 2: SIGINT via background job
mosqueeze-bench -d /tmp/corpus -a all -o /tmp/out &
BENCH_PID=$!
sleep 5
kill -INT $BENCH_PID
wait $BENCH_PID
echo "Exit code: $?"
# Verify: exit code 130, partial results exist
```

### Unit Tests

```cpp
TEST(SignalHandling, SetsInterruptFlag) {
    std::atomic<bool> interrupted{false};
    
    // Simulate signal
    signalHandler(SIGTERM);
    
    EXPECT_TRUE(interrupted.load());
}

TEST(BenchmarkRunner, StopsOnInterrupt) {
    BenchmarkRunner runner;
    registerDefaultEngines(runner);
    
    std::atomic<bool> interrupted{false};
    
    // Start benchmark in thread
    auto future = std::async([&]() {
        return runner.runWithConfig(config, interrupted);
    });
    
    // Simulate interrupt
    interrupted.store(true);
    
    auto results = future.get();
    
    // Should have partial results
    EXPECT_GT(results.size(), 0);
    EXPECT_LT(results.size(), config.files.size());
}
```

---

## Edge Cases

1. **Signal during SQLite write**
   - SQLite transactions are atomic — either committed or not
   - Partial transaction rolled back, next run starts fresh

2. **Signal during file read**
   - No corruption — file handles closed by OS on exit
   - Partial in-memory results lost, but already-processed files saved

3. **Multiple rapid signals**
   - Only first signal triggers flush
   - Second signal terminates immediately (standard Unix behavior)

---

## Migration Notes

- **No breaking changes** — New functionality only
- **Output format**: JSON now has `interrupted` field (backward compatible)
- **Exit codes**: New specific codes for interrupted runs

---

## Rollback Plan

If issues arise:
1. Remove signal handler from `main.cpp`
2. Remove interrupt parameter from `runWithConfig()`
3. Process terminates immediately on signal (original behavior)

---

## References

- Original bug report: [Issue #108](https://github.com/fathorMB/MoSqueeze/issues/108)
- Similar implementations: pytest `--timeout`, gzip signal handling
- Standard exit codes: 128 + signal (130 = SIGINT, 143 = SIGTERM)

---

## Notes for Implementer

- Use `std::atomic<bool>` for thread-safe interrupt flag
- Signal handler should be minimal — just set flag
- Main loop checks flag and handles cleanup
- Consider `std::quick_exit()` for hard exit if flush fails
- Test with both `timeout` and manual Ctrl+C