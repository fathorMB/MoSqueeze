# Worker Spec: ZPAQ File Size Validation

**Issue**: [#107](https://github.com/fathorMB/MoSqueeze/issues/107)  
**Type**: Bug Fix  
**Priority**: P1-Critical  
**Severity**: Critical  
**Estimated Effort**: 2-3 hours

---

## Summary

MoSqueeze applies `zpaq level 3+` to files >2MB without validation, causing unacceptable benchmark durations (hours instead of minutes). This violates documented constraints and blocks CI/CD pipelines.

---

## Problem Statement

### Current Behavior

```cpp
// ZpaqEngine.cpp - No size validation
std::vector<int> ZpaqEngine::supportedLevels() const {
    return {1, 2, 3, 4, 5};  // All levels available regardless of file size
}
```

```cpp
// BenchmarkRunner.cpp - No pre-execution check
for (const int level : levelsByAlgorithm.at(engine->name())) {
    // Runs zpaq/3 on 12MB file without warning
}
```

### Impact

| File Size | zpaq Level | Encode Time | Acceptable? |
|-----------|------------|-------------|-------------|
| 500KB | 3 | ~200ms | ✅ OK |
| 2MB | 3 | ~800ms | ✅ OK |
| 12MB | 3 | **~9 seconds** | ❌ Too slow |
| 12MB | 5 | **~30+ seconds** | ❌ Unacceptable |

On a corpus of 50 files >2MB:
- `zpaq/3`: 50 × 9s = **7.5 minutes**
- `zpaq/5`: 50 × 30s = **25 minutes**

This blocks CI/CD and wastes compute resources.

---

## Root Cause

1. **ZpaqEngine** exposes all levels 1-5 unconditionally
2. **BenchmarkRunner** schedules all algorithm/level combinations without size checks
3. **No warning mechanism** exists for slow combinations

---

## Solution

### Design Decision

Validation belongs in **BenchmarkRunner**, not ZpaqEngine. Rationale:
- ZpaqEngine is a generic compression library — should not impose application-specific constraints
- BenchmarkRunner owns the scheduling logic and can make informed decisions
- Allows future extension for other algorithm-specific constraints

### Implementation

#### 1. File Size Constraint System

Add a configurable constraint registry in `BenchmarkRunner`:

```cpp
// In BenchmarkRunner.hpp
struct AlgorithmConstraint {
    size_t maxFileSize = 0;  // 0 = unlimited
    int maxLevelForSize = 0; // For files > maxFileSize, limit to this level
};

class BenchmarkRunner {
    // ...
    std::unordered_map<std::string, AlgorithmConstraint> constraints_;
};
```

#### 2. ZPAQ-Specific Constraint

Hardcode for now (configurable in future):

```cpp
// In BenchmarkRunner constructor or registerDefaultConstraints()
constraints_["zpaq"] = AlgorithmConstraint{
    .maxFileSize = 2 * 1024 * 1024,  // 2MB
    .maxLevelForSize = 1             // zpaq/1 only for files >2MB
};
```

#### 3. Pre-Scheduling Check

In `BenchmarkRunner::runWithConfig()`:

```cpp
for (size_t fileIdx = 0; fileIdx < config.files.size(); ++fileIdx) {
    const auto& file = config.files[fileIdx];
    const size_t fileSize = std::filesystem::file_size(file);
    
    for (auto* engine : selectedEngines) {
        const auto constraintIt = constraints_.find(engine->name());
        
        for (const int level : levelsByAlgorithm.at(engine->name())) {
            // Check constraint
            if (constraintIt != constraints_.end() && 
                constraintIt->second.maxFileSize > 0 &&
                fileSize > constraintIt->second.maxFileSize &&
                level > constraintIt->second.maxLevelForSize) {
                
                if (!skipWarnings.empty()) {
                    std::cerr << "[WARN] Skipping " << engine->name() << "/" << level
                              << " for " << file.filename().string()
                              << " (" << formatBytes(fileSize) << " > "
                              << formatBytes(constraintIt->second.maxFileSize)
                              << "): max allowed level = "
                              << constraintIt->second.maxLevelForSize << "\\n";
                }
                continue;  // Skip this combination
            }
            
            // Schedule benchmark...
        }
    }
}
```

#### 4. Helper Function

```cpp
// In anonymous namespace
std::string formatBytes(size_t bytes) {
    static const char* suffixes[] = {"B", "KB", "MB", "GB"};
    double value = static_cast<double>(bytes);
    size_t suffix = 0;
    while (value >= 1024.0 && suffix < 3) {
        value /= 1024.0;
        ++suffix;
    }
    std::ostringstream out;
    out << std::fixed << std::setprecision(suffix == 0 ? 0 : 1) 
        << value << suffixes[suffix];
    return out.str();
}
```

---

## Files to Modify

| File | Changes |
|------|---------|
| `src/mosqueeze-bench/include/mosqueeze/bench/BenchmarkRunner.hpp` | Add `AlgorithmConstraint` struct and `constraints_` member |
| `src/mosqueeze-bench/src/BenchmarkRunner.cpp` | Add constraint check in scheduling loop |
| `src/mosqueeze-bench/src/main.cpp` | Add `--quiet` flag to suppress warnings (optional) |

---

## Acceptance Criteria

### Must Have

1. **zpaq level >1 is skipped for files >2MB**
   ```bash
   # Create test file
   dd if=/dev/urandom of=/tmp/test_12mb.bin bs=1M count=12
   
   # Run benchmark
   mosqueeze-bench -d /tmp -a zpaq -l 1,2,3,4,5 -o /tmp/out
   
   # Verify: only levels 1 present in results
   sqlite3 /tmp/out/results.sqlite3 "SELECT DISTINCT level FROM benchmark_results WHERE algorithm='zpaq';"
   # Expected output: 1
   ```

2. **Warning is logged for skipped combinations**
   ```
   [WARN] Skipping zpaq/3 for test_12mb.bin (12.0MB > 2.0MB): max allowed level = 1
   ```

3. **Small files still get all zpaq levels**
   ```bash
   # 500KB file
   dd if=/dev/urandom of=/tmp/test_500kb.bin bs=1K count=500
   mosqueeze-bench -d /tmp -a zpaq -l 1,2,3,4,5 -o /tmp/out
   
   # Verify: all levels present
   sqlite3 /tmp/out/results.sqlite3 "SELECT DISTINCT level FROM benchmark_results WHERE algorithm='zpaq';"
   # Expected output: 1, 2, 3, 4, 5
   ```

### Should Have

4. **Other algorithms unaffected**
   ```bash
   # Verify zstd/brotli/xz still run all levels on large files
   ```

5. **`--quiet` flag suppresses warnings** (optional)

### Nice to Have

6. **Configurable constraints via config file**
   ```yaml
   # future: benchmark-config.yaml
   constraints:
     zpaq:
       max_file_size: 2MB
       max_level: 1
   ```

---

## Testing Plan

### Unit Tests

Create `tests/unit/AlgorithmConstraint_test.cpp`:

```cpp
TEST(AlgorithmConstraint, ZpaqLevelsLimitedForLargeFiles) {
    BenchmarkRunner runner;
    registerDefaultEngines(runner);
    
    // Create 12MB file
    auto tmpFile = createTempFile(12 * 1024 * 1024);
    
    auto levels = runner.availableLevels("zpaq");
    auto filteredLevels = runner.filterLevelsBySize("zpaq", levels, tmpFile);
    
    EXPECT_EQ(filteredLevels.size(), 1);
    EXPECT_EQ(filteredLevels[0], 1);
}

TEST(AlgorithmConstraint, ZpaqAllLevelsForSmallFiles) {
    BenchmarkRunner runner;
    registerDefaultEngines(runner);
    
    // Create 500KB file
    auto tmpFile = createTempFile(500 * 1024);
    
    auto levels = runner.availableLevels("zpaq");
    auto filteredLevels = runner.filterLevelsBySize("zpaq", levels, tmpFile);
    
    EXPECT_EQ(filteredLevels.size(), 5);
}
```

### Integration Tests

```bash
# Test with real benchmarks
./scripts/test-zpaq-constraint.sh
```

---

## Migration Notes

- **No breaking changes** — ZpaqEngine API unchanged
- **Benchmark results**: Large file zpaq results will have fewer levels
- **CI/CD**: Benchmarks will complete faster

---

## Rollback Plan

If issues arise:
1. Remove constraint check from `BenchmarkRunner::runWithConfig()`
2. All levels will be scheduled again

---

## References

- Original bug report: [Issue #107](https://github.com/fathorMB/MoSqueeze/issues/107)
- EXPANSION-GUIDE.md: "zpaq ≤1 for files >2MB"
- ZpaqEngine implementation: `src/libmosqueeze/src/engines/ZpaqEngine.cpp`

---

## Notes for Implementer

- Keep warning messages concise but informative
- Use `std::filesystem::file_size()` with error handling
- Consider adding `--skip-constraints` flag for debugging
- Future: Make constraints configurable via CLI or config file