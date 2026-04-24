#include <mosqueeze/PredictionEngine.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace mosqueeze;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static fs::path writeTmp(const std::string& name, const std::string& content) {
    const fs::path p = fs::temp_directory_path() / ("mosqueeze_pe_test_" + name);
    std::ofstream f(p, std::ios::binary);
    f << content;
    return p;
}

// Minimal JPEG magic bytes so FileTypeDetector recognises the file
static fs::path writeTmpJpeg(const std::string& name) {
    const fs::path p = fs::temp_directory_path() / ("mosqueeze_pe_test_" + name);
    std::ofstream f(p, std::ios::binary);
    // JFIF / EXIF header
    const unsigned char hdr[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10,
                                 'J','F','I','F', 0x00};
    f.write(reinterpret_cast<const char*>(hdr), sizeof(hdr));
    return p;
}

static void removeTmp(const fs::path& p) {
    std::error_code ec;
    fs::remove(p, ec);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void testConstructAndStats() {
    PredictionEngine engine;
    const auto stats = engine.getStats();
    assert(stats.totalBenchmarks > 0);
    assert(stats.supportedExtensions > 0);
    assert(!stats.skipExtensions.empty());
    std::cout << "[PASS] stats: " << stats.supportedExtensions
              << " extensions, " << stats.totalBenchmarks
              << " benchmarks, " << stats.skipExtensions.size()
              << " skip\n";
}

void testHasDataFor() {
    PredictionEngine engine;
    assert(engine.hasDataFor(".json"));
    assert(engine.hasDataFor("json"));   // without dot
    assert(engine.hasDataFor(".JSON"));  // case-insensitive
    assert(!engine.hasDataFor(".unknown_zzz"));
    std::cout << "[PASS] hasDataFor\n";
}

void testSupportedExtensions() {
    PredictionEngine engine;
    const auto exts = engine.supportedExtensions();
    assert(!exts.empty());
    bool hasJson = false;
    for (const auto& e : exts) if (e == ".json") { hasJson = true; break; }
    assert(hasJson);
    std::cout << "[PASS] supportedExtensions: " << exts.size() << " entries\n";
}

void testPredictJsonFile() {
    const fs::path p = writeTmp("sample.json",
        "{\"key\":\"value\",\"items\":[1,2,3]}");

    PredictionEngine engine;
    const auto r = engine.predict(p);

    assert(!r.shouldSkip);
    assert(r.extension == ".json");
    assert(!r.recommendations.empty());
    assert(r.primary() != nullptr);
    assert(r.primary()->estimatedRatio > 1.0);
    assert(!r.primary()->algorithm.empty());
    assert(r.preprocessorAvailable);
    assert(r.preprocessor == "json-canonical");
    assert(r.inputSize > 0);
    std::cout << "[PASS] predict json: " << r.primary()->algorithm
              << "/" << r.primary()->level
              << " ratio=" << r.primary()->estimatedRatio << "x"
              << " preprocessor=" << r.preprocessor << "\n";

    removeTmp(p);
}

void testPredictCppFile() {
    const fs::path p = writeTmp("hello.cpp",
        "#include <iostream>\nint main() { std::cout << \"hello\"; }\n");

    PredictionEngine engine;
    const auto r = engine.predict(p);

    assert(!r.shouldSkip);
    assert(r.extension == ".cpp");
    assert(!r.recommendations.empty());
    assert(r.preprocessor == "none");
    std::cout << "[PASS] predict .cpp: " << r.primary()->algorithm
              << "/" << r.primary()->level << "\n";

    removeTmp(p);
}

void testSkipJpeg() {
    const fs::path p = writeTmpJpeg("photo.jpg");

    PredictionEngine engine;
    const auto r = engine.predict(p);

    assert(r.shouldSkip);
    assert(!r.skipReason.empty());
    assert(r.recommendations.empty());
    std::cout << "[PASS] skip jpeg: " << r.skipReason << "\n";

    removeTmp(p);
}

void testProgressCallback() {
    const fs::path p = writeTmp("data.json", "{\"a\":1}");

    PredictionEngine engine;
    std::vector<std::string> stages;
    engine.predict(p, [&](const std::string& stage, double /*progress*/) {
        stages.push_back(stage);
    });

    assert(!stages.empty());
    bool hasComplete = false;
    for (const auto& s : stages) if (s == "complete") { hasComplete = true; break; }
    assert(hasComplete);
    std::cout << "[PASS] progress callback: " << stages.size() << " stages\n";

    removeTmp(p);
}

void testToJsonRoundtrip() {
    const fs::path p = writeTmp("doc.json", "{\"x\":42}");

    PredictionEngine engine;
    const auto r = engine.predict(p);
    const std::string json = r.toJson();

    // Verify it's parseable and has expected keys
    assert(!json.empty());
    assert(json.find("\"extension\"") != std::string::npos);
    assert(json.find("\"recommendations\"") != std::string::npos);

    // Round-trip
    const auto r2 = PredictionResult::fromJson(json);
    assert(r2.extension == r.extension);
    assert(r2.shouldSkip == r.shouldSkip);
    assert(r2.recommendations.size() == r.recommendations.size());
    if (!r2.recommendations.empty()) {
        assert(r2.recommendations[0].algorithm == r.recommendations[0].algorithm);
        assert(r2.recommendations[0].level     == r.recommendations[0].level);
    }
    std::cout << "[PASS] toJson/fromJson roundtrip\n";

    removeTmp(p);
}

void testBatchPredict() {
    const fs::path p1 = writeTmp("a.json", "{\"a\":1}");
    const fs::path p2 = writeTmp("b.cpp", "int main(){}");

    PredictionEngine engine;
    int batchCalls = 0;
    const auto results = engine.predictBatch(
        {p1, p2},
        [&](const std::string& stage, double) {
            if (stage == "batch") ++batchCalls;
        });

    assert(results.size() == 2);
    assert(results[0].extension == ".json");
    assert(results[1].extension == ".cpp");
    assert(batchCalls >= 1);
    std::cout << "[PASS] predictBatch: " << results.size() << " results\n";

    removeTmp(p1);
    removeTmp(p2);
}

void testPreferSpeedConfig() {
    const fs::path p = writeTmp("big.json",
        std::string(1000, '{') + "\"k\":\"v\"" + std::string(1000, '}'));

    PredictionConfig cfg;
    cfg.preferSpeed = true;

    PredictionEngine engine;
    engine.setConfig(cfg);
    const auto r = engine.predict(p);

    if (!r.shouldSkip && r.recommendations.size() > 1) {
        // When preferSpeed=true, fastest option should come first
        const auto* first = r.primary();
        const auto* second = r.fastest();
        assert(first != nullptr && second != nullptr);
        // First should be at least as fast as second
        assert(static_cast<int>(first->speed) <= static_cast<int>(second->speed));
    }
    std::cout << "[PASS] preferSpeed config\n";

    removeTmp(p);
}

void testCustomDecisionMatrixPath() {
#ifndef DECISION_MATRIX_JSON_PATH
    std::cout << "[SKIP] custom matrix path (DECISION_MATRIX_JSON_PATH not set)\n";
    return;
#else
    const std::string pathStr = DECISION_MATRIX_JSON_PATH;
    if (pathStr.empty()) {
        std::cout << "[SKIP] custom matrix path (empty)\n";
        return;
    }
    PredictionConfig cfg;
    cfg.decisionMatrixPath = pathStr;
    PredictionEngine engine;
    engine.setConfig(cfg);
    assert(engine.getStats().supportedExtensions > 0);
    std::cout << "[PASS] custom decisionMatrixPath\n";
#endif
}

// ---------------------------------------------------------------------------

int main() {
    testConstructAndStats();
    testHasDataFor();
    testSupportedExtensions();
    testPredictJsonFile();
    testPredictCppFile();
    testSkipJpeg();
    testProgressCallback();
    testToJsonRoundtrip();
    testBatchPredict();
    testPreferSpeedConfig();
    testCustomDecisionMatrixPath();

    std::cout << "[PASS] All PredictionEngine tests passed\n";
    return 0;
}
