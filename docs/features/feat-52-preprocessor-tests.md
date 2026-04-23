# Worker Spec: Preprocessor Unit Test Suite

**Issue:** #52 - Feature: Preprocessor Unit Test Suite  
**Branch:** `feat/preprocessor-tests`  
**Priority:** Medium  
**Estimated Effort:** 1-2 days  

---

## Overview

Create comprehensive unit tests for all preprocessors to prevent regressions and validate correctness. This addresses the BayerPreprocessor bug (#46) where the entire RAF file was processed instead of just image data - unit tests would have caught this.

---

## Goals

1. **Correctness validation** - Ensure round-trip integrity for all preprocessors
2. **Edge case handling** - Test malformed input, empty files, large files
3. **Performance benchmarks** - Establish baseline performance metrics
4. **CI integration** - Add to build pipeline with coverage reporting

---

## Test Categories

### 1. Correctness Tests (Round-trip)

Each preprocessor must ensure data integrity:

```cpp
// For lossless preprocessors (BayerPreprocessor, PngOptimizer, ImageMetaStripper)
TEST_CASE("BayerPreprocessor round-trip") {
    auto original = loadRafFile("test.raf");
    auto processed = BayerPreprocessor::process(original);
    auto decompressed = decompress(compress(processed));
    auto restored = BayerPreprocessor::restore(decompressed);
    REQUIRE(original == restored);  // Pixel-perfect
}

// For canonicalizers (JsonCanonicalizer, XmlCanonicalizer)
TEST_CASE("JsonCanonicalizer semantic equivalence") {
    auto json1 = R"({"b":2,"a":1})"_json;
    auto json2 = R"({"a":1,"b":2})"_json;
    auto canonical = JsonCanonicalizer::process(json1);
    REQUIRE(canonical == JsonCanonicalizer::process(json2));
    REQUIRE(parse(canonical) == json1);  // Semantic equivalence
}
```

### 2. Edge Cases

```cpp
TEST_CASE("Empty file handling") {
    REQUIRE_NOTHROW(BayerPreprocessor::process(std::vector<uint8_t>{}));
    REQUIRE_NOTHROW(PngOptimizer::process(std::vector<uint8_t>{}));
}

TEST_CASE("Malformed input") {
    auto garbage = generateRandomBytes(1024);
    REQUIRE_THROWS_AS(PngOptimizer::process(garbage), InvalidFormatError);
    REQUIRE_NOTHROW(ImageMetaStripper::process(garbage));  // Best-effort stripping
}

TEST_CASE("Large file (>1GB)") {
    // Test with streaming mock or subset
    auto large = generateLargeRaf(1.5 * 1024 * 1024 * 1024);
    REQUIRE_NOTHROW(BayerPreprocessor::process(large));
}

TEST_CASE("Already optimally compressed") {
    auto optimized7z = loadFile("optimized.7z");
    auto processed = PreprocessorChain::process(optimized7z);
    REQUIRE(processed.size() >= optimized7z.size());  // No improvement
}
```

### 3. Format-Specific Tests

#### BayerPreprocessor
```cpp
TEST_CASE("BayerPreprocessor: RAF format detection")
TEST_CASE("BayerPreprocessor: Header preservation")
TEST_CASE("BayerPreprocessor: Only image data processed")
TEST_CASE("BayerPreprocessor: Lossless-compressed skip")
TEST_CASE("BayerPreprocessor: All Bayer patterns (RGGB, BGGR, GRBG, GBRG)")
```

#### PngOptimizer
```cpp
TEST_CASE("PngOptimizer: Filter selection (0-4)")
TEST_CASE("PngOptimizer: Metadata stripping")
TEST_CASE("PngOptimizer: Compression level comparison")
TEST_CASE("PngOptimizer: Palette optimization")
TEST_CASE("PngOptimizer: Interlaced PNG handling")
```

#### ImageMetaStripper
```cpp
TEST_CASE("ImageMetaStripper: JPEG EXIF removal")
TEST_CASE("ImageMetaStripper: PNG XMP removal")
TEST_CASE("ImageMetaStripper: WebP ICC profile removal")
TEST_CASE("ImageMetaStripper: Preserve image data")
```

#### JsonCanonicalizer
```cpp
TEST_CASE("JsonCanonicalizer: Key ordering")
TEST_CASE("JsonCanonicalizer: Whitespace normalization")
TEST_CASE("JsonCanonicalizer: Unicode handling")
TEST_CASE("JsonCanonicalizer: Nested objects")
TEST_CASE("JsonCanonicalizer: Arrays preserved")
```

#### XmlCanonicalizer
```cpp
TEST_CASE("XmlCanonicalizer: Attribute ordering")
TEST_CASE("XmlCanonicalizer: Namespace handling")
TEST_CASE("XmlCanonicalizer: Whitespace normalization")
TEST_CASE("XmlCanonicalizer: CDATA sections")
```

---

## Proposed Test Suite Structure

```
tests/
├── CMakeLists.txt
├── test_main.cpp
├── preprocessors/
│   ├── BayerPreprocessor_test.cpp
│   ├── PngOptimizer_test.cpp
│   ├── ImageMetaStripper_test.cpp
│   ├── JsonCanonicalizer_test.cpp
│   ├── XmlCanonicalizer_test.cpp
│   └── PreprocessorChain_test.cpp
├── fixtures/
│   ├── images/
│   │   ├── sample.raf          # Fujifilm RAW
│   │   ├── sample.png          # Test PNG with metadata
│   │   ├── sample.jpg          # Test JPEG with EXIF
│   │   └── sample.webp         # Test WebP
│   ├── data/
│   │   ├── sample.json         # Test JSON
│   │   ├── sample.xml          # Test XML
│   │   └── malformed.*         # Malformed test cases
│   └── edge_cases/
│       ├── empty.bin
│       ├── tiny.bin            # 1 byte
│       └── large.bin           # 10MB+
└── integration/
    ├── round_trip_test.cpp
    └── compression_pipeline_test.cpp
```

---

## Test Fixtures

### Required Test Files

1. **sample.raf** - Fujifilm RAW (can use existing benchmark files)
2. **sample.png** - PNG with EXIF, XMP, ICC profile
3. **sample.jpg** - JPEG with full EXIF
4. **sample.json** - JSON with nested objects, arrays, unicode
5. **sample.xml** - XML with namespaces, attributes
6. **malformed.*** - Edge case files for error handling

### Fixture Generation

```cpp
// Generate test fixtures if not present
void ensureTestFixtures() {
    if (!fs::exists("fixtures/images/sample.png")) {
        generateTestPng("fixtures/images/sample.png", 100, 100, {
            .withExif = true,
            .withXmp = true,
            .withIcc = true
        });
    }
    // Similar for other formats
}
```

---

## Performance Tests

```cpp
class PreprocessorBenchmark : public Catch::BenchmarkListener {
    TEST_CASE("BayerPreprocessor performance", "[!benchmark]") {
        auto file = loadRafFile("large.raf");
        BENCHMARK("Process 50MB RAF") {
            return BayerPreprocessor::process(file);
        };
    }
    
    TEST_CASE("PngOptimizer performance", "[!benchmark]") {
        auto png = loadPngFile("large.png");
        BENCHMARK("Optimize 10MB PNG") {
            return PngOptimizer::process(png);
        };
    }
};
```

### Performance Baselines

| Preprocessor | File Size | Target Time |
|--------------|-----------|-------------|
| BayerPreprocessor | 50MB RAF | <500ms |
| PngOptimizer | 10MB PNG | <1s (level 9) |
| JsonCanonicalizer | 1MB JSON | <50ms |
| XmlCanonicalizer | 1MB XML | <50ms |
| ImageMetaStripper | 5MB JPEG | <100ms |

---

## CI Integration

### CMakeLists.txt Additions

```cmake
# Test configuration
enable_testing()

# Catch2 setup
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(Catch2)

# Test executable
add_executable(mosqueeze_tests
    tests/test_main.cpp
    tests/preprocessors/BayerPreprocessor_test.cpp
    tests/preprocessors/PngOptimizer_test.cpp
    tests/preprocessors/ImageMetaStripper_test.cpp
    tests/preprocessors/JsonCanonicalizer_test.cpp
    tests/preprocessors/XmlCanonicalizer_test.cpp
)

target_link_libraries(mosqueeze_tests PRIVATE
    mosqueeze_lib
    Catch2::Catch2WithMain
)

# Register tests
include(Catch)
catch_discover_tests(mosqueeze_tests)
```

### GitHub Actions Workflow Addition

```yaml
# Add to existing CI workflow
- name: Run Tests
  run: |
    cmake --build build --config Release
    ctest --test-dir build --output-on-failure
  
- name: Coverage Report
  if: matrix.os == 'ubuntu-latest'
  run: |
    cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
    cmake --build build
    ctest --test-dir build
    bash <(curl -s https://codecov.io/bash)
```

---

## Memory Tests

Valgrind / AddressSanitizer integration:

```cmake
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN)
    target_compile_options(mosqueeze_tests PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(mosqueeze_tests PRIVATE -fsanitize=address)
endif()
```

---

## Implementation Steps

### Phase 1: Setup (2-3 hours)
1. [ ] Add Catch2 dependency via FetchContent
2. [ ] Create test directory structure
3. [ ] Set up test main and CMake integration
4. [ ] Add `ctest` to CI workflow

### Phase 2: Test Fixtures (1-2 hours)
1. [ ] Add minimal test fixtures (empty files, valid samples)
2. [ ] Generate malformed test files
3. [ ] Add fixture generation utilities

### Phase 3: Correctness Tests (4-6 hours)
1. [ ] BayerPreprocessor correctness tests
2. [ ] PngOptimizer correctness tests
3. [ ] ImageMetaStripper correctness tests
4. [ ] JsonCanonicalizer correctness tests
5. [ ] XmlCanonicalizer correctness tests

### Phase 4: Edge Cases (2-3 hours)
1. [ ] Empty file tests
2. [ ] Malformed input tests
3. [ ] Large file tests (streaming)
4. [ ] Already-optimized tests

### Phase 5: Performance (1-2 hours)
1. [ ] Add benchmark macros
2. [ ] Establish baseline measurements
3. [ ] Add performance regression tests

### Phase 6: CI Integration (1 hour)
1. [ ] Add coverage reporting
2. [ ] Add ASAN builds
3. [ ] Update README with test commands

---

## Test Commands

```bash
# Build tests
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure

# Run specific test suite
./build/tests/mosqueeze_tests "[BayerPreprocessor]"

# Run with verbose output
./build/tests/mosqueeze_tests -s

# Run benchmarks
./build/tests/mosqueeze_tests "[!benchmark]"

# Coverage
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
cmake --build build
ctest --test-dir build
gcovr -r . build
```

---

## Acceptance Criteria

- [ ] Unit tests for all 5 preprocessors
- [ ] Round-trip correctness verified for lossless preprocessors
- [ ] Semantic equivalence verified for canonicalizers
- [ ] Edge case handling (empty, malformed, large)
- [ ] Performance baselines established
- [ ] CI integration with coverage reporting
- [ ] Test fixtures for common file formats
- [ ] README updated with test commands
- [ ] All tests passing on CI (Linux, macOS, Windows)

---

## Related Issues

- #46: BayerPreprocessor RAF parsing bug (would have been caught)
- #13: PNG optimization (tests needed)
- #51: oxipng integration (tests needed)

---

## Testing Philosophy

> "Tests don't prove code works; they prove code doesn't break in tested ways."

This test suite focuses on:
1. **Regression prevention** - If BayerPreprocessor breaks again, tests catch it
2. **Documentation** - Tests show how preprocessors should behave
3. **Confidence** - Refactor safely with test coverage
4. **Performance tracking** - Catch performance regressions early

---

## Dependencies

- **Catch2 v3.x** - Test framework (header-only option available)
- **CTest** - CMake test runner (built-in)
- **gcov/codecov** - Coverage reporting (optional)

---

## Notes

- This is a foundational improvement that all future preprocessor work benefits from
- Tests should be co-located with implementation in `tests/preprocessors/`
- Use `TEST_CASE_METHOD` for shared fixture setup
- Mark slow tests with `[!mayfail]` or `[.]` tags for filtering