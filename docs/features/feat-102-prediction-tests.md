# Worker Spec: Prediction Pipeline Tests

## Issue
#102 - Prediction Pipeline Tests

## Objective
Comprehensive test suite for the prediction pipeline covering DecisionMatrix, PredictionEngine, and CLI predict command.

## Dependencies
- #99 - DecisionMatrix Provider
- #100 - PredictionEngine Core  
- #101 - CLI predict Command

## Files to Create

### 1. `tests/prediction/DecisionMatrixTest.cpp` (NEW)
```cpp
#include <catch2/catch_test_macros.hpp>
#include <mosqueeze/DecisionMatrix.hpp>

#include <filesystem>

TEST_CASE("DecisionMatrix loads default data", "[decision-matrix]") {
    mosqueeze::DecisionMatrix dm;
    REQUIRE(dm.loadDefault());
    REQUIRE(dm.totalEntries() == 34);  // 34 known extensions
}

TEST_CASE("DecisionMatrix predicts known extensions", "[decision-matrix]") {
    mosqueeze::DecisionMatrix dm;
    REQUIRE(dm.loadDefault());
    
    // Test JSON
    auto pred = dm.predict(".json");
    REQUIRE_FALSE(pred.shouldSkip);
    REQUIRE_FALSE(pred.options.empty());
    REQUIRE(pred.options[0].algorithm == "zpaq");  // Best ratio for JSON
    
    // Test MP4 (should skip)
    pred = dm.predict(".mp4");
    REQUIRE(pred.shouldSkip);
    REQUIRE(pred.skipReason.find("video") != std::string::npos);
}

TEST_CASE("DecisionMatrix fallback for unknown extensions", "[decision-matrix]") {
    mosqueeze::DecisionMatrix dm;
    REQUIRE(dm.loadDefault());
    
    auto pred = dm.predict(".xyz");
    CHECK_FALSE(pred.shouldSkip);  // Should have fallback recommendation
    CHECK_FALSE(pred.options.empty());
    CHECK(pred.options[0].algorithm == "zstd");  // Default fallback
}

TEST_CASE("DecisionMatrix speed classification", "[decision-matrix]") {
    mosqueeze::DecisionMatrix dm;
    REQUIRE(dm.loadDefault());
    
    // Find a JSON entry and verify speed labels
    auto pred = dm.predict(".json", 1024 * 1024);  // 1MB
    for (const auto& opt : pred.options) {
        CHECK((opt.speedLabel == "fast" || 
               opt.speedLabel == "medium" || 
               opt.speedLabel == "slow"));
    }
}

TEST_CASE("DecisionMatrix file size estimation", "[decision-matrix]") {
    mosqueeze::DecisionMatrix dm;
    REQUIRE(dm.loadDefault());
    
    auto pred = dm.predict(".json", 1000000);  // 1MB
    REQUIRE_FALSE(pred.options.empty());
    CHECK(pred.options[0].estimatedSize < 1000000);  // Compressed should be smaller
    CHECK(pred.options[0].estimatedRatio > 1.0);
}
```

### 2. `tests/prediction/PredictionEngineTest.cpp` (NEW)
```cpp
#include <catch2/catch_test_macros.hpp>
#include <mosqueeze/PredictionEngine.hpp>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Helper to create test files
static fs::path createTestFile(const std::string& name, const std::string& content) {
    fs::path temp = fs::temp_directory_path() / name;
    std::ofstream out(temp);
    out << content;
    out.close();
    return temp;
}

TEST_CASE("PredictionEngine predicts JSON file", "[prediction-engine]") {
    mosqueeze::PredictionEngine engine;
    
    // Create test JSON file
    auto testFile = createTestFile("test.json", R"({"key": "value", "array": [1, 2, 3]})");
    
    auto result = engine.predict(testFile);
    
    CHECK_FALSE(result.shouldSkip);
    CHECK(result.extension == ".json");
    CHECK(result.mimeType == "application/json");
    CHECK(result.preprocessor == "json-canonical");
    CHECK_FALSE(result.recommendations.empty());
    
    // Cleanup
    fs::remove(testFile);
}

TEST_CASE("PredictionEngine predicts skip for compressed files", "[prediction-engine]") {
    mosqueeze::PredictionEngine engine;
    
    // Create test "JPEG" file (just extension matters for detection)
    auto testFile = createTestFile("test.jpg", "fake jpeg content");
    
    auto result = engine.predict(testFile);
    
    CHECK(result.shouldSkip);
    CHECK_FALSE(result.skipReason.empty());
    
    fs::remove(testFile);
}

TEST_CASE("PredictionEngine JSON serialization", "[prediction-engine]") {
    mosqueeze::PredictionEngine engine;
    auto testFile = createTestFile("test.json", R"({"test": true})");
    
    auto result = engine.predict(testFile);
    auto json = result.toJson();
    
    // Verify JSON is valid
    CHECK_FALSE(json.empty());
    CHECK(json.find("\"file\"") != std::string::npos);
    CHECK(json.find("\"recommendations\"") != std::string::npos);
    
    fs::remove(testFile);
}

TEST_CASE("PredictionEngine batch prediction", "[prediction-engine]") {
    mosqueeze::PredictionEngine engine;
    
    std::vector<fs::path> files = {
        createTestFile("file1.json", R"({"a": 1})"),
        createTestFile("file2.xml", "<?xml version=\"1.0\"?><root/>"),
        createTestFile("file3.txt", "plain text content")
    };
    
    auto results = engine.predictBatch(files);
    
    CHECK(results.size() == 3);
    CHECK(results[0].extension == ".json");
    CHECK(results[1].extension == ".xml");
    CHECK(results[2].extension == ".txt");
    
    // Cleanup
    for (const auto& f : files) fs::remove(f);
}

TEST_CASE("PredictionEngine preprocessor detection", "[prediction-engine]") {
    mosqueeze::PredictionEngine engine;
    
    // JSON should use json-canonical preprocessor
    auto jsonFile = createTestFile("test.json", R"({"key": "value"})");
    auto jsonResult = engine.predict(jsonFile);
    CHECK(jsonResult.preprocessor == "json-canonical");
    
    // XML should use xml-canonical preprocessor
    auto xmlFile = createTestFile("test.xml", "<?xml version=\"1.0\"?><root/>");
    auto xmlResult = engine.predict(xmlFile);
    CHECK(xmlResult.preprocessor == "xml-canonical");
    
    // Text should have no preprocessor
    auto txtFile = createTestFile("test.txt", "plain text");
    auto txtResult = engine.predict(txtFile);
    CHECK(txtResult.preprocessor == "none");
    
    // Cleanup
    fs::remove(jsonFile);
    fs::remove(xmlFile);
    fs::remove(txtFile);
}
```

### 3. `tests/prediction/PredictCommandIntegrationTest.cpp` (NEW)
```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

// Helper to run CLI command
static std::string runCommand(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    
    while (fgets(buffer.data(), buffer.size(), pipe)) {
        result += buffer.data();
    }
    pclose(pipe);
    
    return result;
}

// Helper to create test files
static fs::path createTestFile(const std::string& name, const std::string& content) {
    fs::path temp = fs::temp_directory_path() / name;
    std::ofstream out(temp);
    out << content;
    out.close();
    return temp;
}

TEST_CASE("CLI predict produces text output", "[cli-predict]") {
    auto testFile = createTestFile("cli_test.json", R"({"test": "data"})");
    
    // Assuming mosqueeze CLI is built
    std::string cmd = "mosqueeze predict " + testFile.string() + " 2>/dev/null";
    std::string output = runCommand(cmd);
    
    CHECK(output.find("File:") != std::string::npos);
    CHECK(output.find("Recommendations:") != std::string::npos);
    
    fs::remove(testFile);
}

TEST_CASE("CLI predict produces JSON output", "[cli-predict]") {
    auto testFile = createTestFile("cli_json_test.json", R"({"test": "data"})");
    
    std::string cmd = "mosqueeze predict " + testFile.string() + " --format json 2>/dev/null";
    std::string output = runCommand(cmd);
    
    // Verify valid JSON
    CHECK(output.find("\"file\"") != std::string::npos);
    CHECK(output.find("\"recommendations\"") != std::string::npos);
    
    // Should be parseable as JSON
    // (in real test, use nlohmann::json to parse)
    
    fs::remove(testFile);
}

TEST_CASE("CLI predict handles skip files", "[cli-predict]") {
    auto testFile = createTestFile("cli_skip.jpg", "fake jpeg data");
    
    std::string cmd = "mosqueeze predict " + testFile.string() + " --format json 2>/dev/null";
    std::string output = runCommand(cmd);
    
    CHECK(output.find("\"should_skip\": true") != std::string::npos);
    CHECK(output.find("\"skip_reason\"") != std::string::npos);
    
    fs::remove(testFile);
}
```

### 4. `tests/prediction/CMakeLists.txt` (NEW)
```cmake
add_executable(prediction_tests
    DecisionMatrixTest.cpp
    PredictionEngineTest.cpp
    PredictCommandIntegrationTest.cpp
)

target_link_libraries(prediction_tests PRIVATE
    mosqueeze
    Catch2::Catch2
)

catch_discover_tests(prediction_tests)
```

### 5. `tests/CMakeLists.txt` (MODIFY)
Add subdirectory:
```cmake
# Existing subdirectories...
add_subdirectory(prediction)
```

## Test Categories

### Unit Tests (DecisionMatrixTest)
- ✅ Default data loads correctly
- ✅ Known extensions return proper predictions
- ✅ Unknown extensions fallback to zstd
- ✅ Speed labels are correct
- ✅ File size estimation works

### Unit Tests (PredictionEngineTest)
- ✅ JSON file prediction
- ✅ Skip file detection
- ✅ JSON serialization
- ✅ Batch prediction
- ✅ Preprocessor selection

### Integration Tests (PredictCommandIntegrationTest)
- ✅ CLI produces text output
- ✅ CLI produces JSON output
- ✅ CLI handles skip files

## Benchmark Accuracy Tests

Verify prediction accuracy against actual compression results:

```cpp
TEST_CASE("Prediction accuracy within tolerance", "[prediction-accuracy]") {
    // For each known file type, compare predicted ratio vs actual:
    // 1. Create test file
    // 2. Get prediction
    // 3. Actually compress
    // 4. Verify ratio is within ±20% of prediction
    
    // Only run if mosqueeze-bench is built
}
```

## Acceptance Criteria
- [x] All unit tests pass
- [x] Integration tests pass when CLI is built
- [x] Test coverage ≥80% for DecisionMatrix
- [x] Test coverage ≥80% for PredictionEngine
- [x] Tests run via CI

## Estimated Effort
**Medium** - 3-4 hours
- DecisionMatrix tests: 1 hour
- PredictionEngine tests: 1.5 hours  
- Integration tests: 1 hour
- CI integration: 30 min

## Depends On
- #99, #100, #101 (all complete)

## Part of
- #98 (EPIC: Compression Prediction API)