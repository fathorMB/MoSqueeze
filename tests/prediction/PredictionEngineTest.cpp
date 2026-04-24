#include <catch2/catch_test_macros.hpp>
#include <mosqueeze/PredictionEngine.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace mosqueeze;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static fs::path writeTmp(const std::string& name, const std::string& content) {
    const fs::path p = fs::temp_directory_path() / ("pe_test_" + name);
    std::ofstream f(p, std::ios::binary);
    f << content;
    return p;
}

// Minimal valid JPEG header so FileTypeDetector recognises magic bytes
static fs::path writeTmpJpeg(const std::string& name) {
    const fs::path p = fs::temp_directory_path() / ("pe_test_" + name);
    std::ofstream f(p, std::ios::binary);
    const unsigned char hdr[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10,
                                  'J', 'F', 'I', 'F', 0x00};
    f.write(reinterpret_cast<const char*>(hdr), sizeof(hdr));
    return p;
}

// RAII cleanup so a failing test still removes tmp files
struct TmpFile {
    fs::path path;
    explicit TmpFile(fs::path p) : path(std::move(p)) {}
    ~TmpFile() { std::error_code ec; fs::remove(path, ec); }
};

// ---------------------------------------------------------------------------
// Construction & stats
// ---------------------------------------------------------------------------

TEST_CASE("PredictionEngine constructs and loads data", "[prediction-engine]") {
    PredictionEngine engine;
    const auto stats = engine.getStats();
    REQUIRE(stats.totalBenchmarks > 0);
    REQUIRE(stats.supportedExtensions == 34);
    REQUIRE_FALSE(stats.skipExtensions.empty());
}

TEST_CASE("PredictionEngine hasDataFor known extension", "[prediction-engine]") {
    PredictionEngine engine;
    CHECK(engine.hasDataFor(".json"));
    CHECK(engine.hasDataFor("json"));
    CHECK(engine.hasDataFor(".JSON"));
    CHECK_FALSE(engine.hasDataFor(".unknown_zzz_test"));
}

TEST_CASE("PredictionEngine supportedExtensions", "[prediction-engine]") {
    PredictionEngine engine;
    const auto exts = engine.supportedExtensions();
    CHECK(exts.size() == 34);
    const bool hasJson = std::find(exts.begin(), exts.end(), ".json") != exts.end();
    CHECK(hasJson);
}

// ---------------------------------------------------------------------------
// predict() — compressible files
// ---------------------------------------------------------------------------

TEST_CASE("PredictionEngine predicts JSON file", "[prediction-engine]") {
    TmpFile f{writeTmp("sample.json", R"({"key":"value","array":[1,2,3]})")};
    PredictionEngine engine;
    const auto r = engine.predict(f.path);

    CHECK_FALSE(r.shouldSkip);
    CHECK(r.extension == ".json");
    REQUIRE_FALSE(r.recommendations.empty());
    CHECK(r.primary() != nullptr);
    CHECK(r.primary()->estimatedRatio > 1.0);
    CHECK(!r.primary()->algorithm.empty());
    CHECK(r.inputSize > 0);
}

TEST_CASE("PredictionEngine predicts XML file", "[prediction-engine]") {
    TmpFile f{writeTmp("config.xml",
        "<?xml version=\"1.0\"?><root><item key=\"val\"/></root>")};
    PredictionEngine engine;
    const auto r = engine.predict(f.path);

    CHECK_FALSE(r.shouldSkip);
    CHECK(r.extension == ".xml");
    CHECK_FALSE(r.recommendations.empty());
}

TEST_CASE("PredictionEngine predicts .cpp source file", "[prediction-engine]") {
    TmpFile f{writeTmp("hello.cpp",
        "#include <iostream>\nint main(){std::cout<<\"hi\";}\n")};
    PredictionEngine engine;
    const auto r = engine.predict(f.path);

    CHECK_FALSE(r.shouldSkip);
    CHECK(r.extension == ".cpp");
    CHECK_FALSE(r.recommendations.empty());
    CHECK(r.preprocessor == "none");
}

TEST_CASE("PredictionEngine populates inputSize and estimatedOutputSize", "[prediction-engine]") {
    TmpFile f{writeTmp("data.json", std::string(5000, 'a'))};
    PredictionEngine engine;
    const auto r = engine.predict(f.path);

    REQUIRE_FALSE(r.shouldSkip);
    CHECK(r.inputSize == 5000);
    CHECK(r.estimatedOutputSize < r.inputSize); // must compress
}

// ---------------------------------------------------------------------------
// predict() — skip files
// ---------------------------------------------------------------------------

TEST_CASE("PredictionEngine skips JPEG by magic bytes", "[prediction-engine]") {
    TmpFile f{writeTmpJpeg("photo.jpg")};
    PredictionEngine engine;
    const auto r = engine.predict(f.path);

    CHECK(r.shouldSkip);
    CHECK_FALSE(r.skipReason.empty());
    CHECK(r.recommendations.empty());
}

TEST_CASE("PredictionEngine skips .mp4 by decision matrix", "[prediction-engine]") {
    // .mp4 with no video magic bytes → falls through to decision matrix skip
    TmpFile f{writeTmp("clip.mp4", "not really a video")};
    PredictionEngine engine;
    const auto r = engine.predict(f.path);

    CHECK(r.shouldSkip);
}

// ---------------------------------------------------------------------------
// Preprocessor detection
// ---------------------------------------------------------------------------

TEST_CASE("PredictionEngine preprocessor detection", "[prediction-engine]") {
    PredictionEngine engine;

    SECTION("JSON gets json-canonical") {
        TmpFile f{writeTmp("t.json", R"({"k":"v"})")};
        const auto r = engine.predict(f.path);
        CHECK(r.preprocessor == "json-canonical");
        CHECK(r.preprocessorAvailable);
    }

    SECTION("XML gets xml-canonical") {
        TmpFile f{writeTmp("t.xml", "<?xml version=\"1.0\"?><r/>")};
        const auto r = engine.predict(f.path);
        CHECK(r.preprocessor == "xml-canonical");
        CHECK(r.preprocessorAvailable);
    }

    SECTION("Plain text gets none") {
        TmpFile f{writeTmp("t.txt", "hello world")};
        const auto r = engine.predict(f.path);
        CHECK(r.preprocessor == "none");
        CHECK_FALSE(r.preprocessorAvailable);
    }
}

TEST_CASE("PredictionEngine disables preprocessing when configured", "[prediction-engine]") {
    TmpFile f{writeTmp("t2.json", R"({"x":1})")};
    PredictionConfig cfg;
    cfg.enablePreprocessing = false;
    PredictionEngine engine;
    engine.setConfig(cfg);
    const auto r = engine.predict(f.path);

    CHECK(r.preprocessor == "none");
    CHECK_FALSE(r.preprocessorAvailable);
}

// ---------------------------------------------------------------------------
// preferSpeed config
// ---------------------------------------------------------------------------

TEST_CASE("PredictionEngine preferSpeed reorders recommendations", "[prediction-engine]") {
    // .json has fast + slow options → with preferSpeed the fastest comes first
    TmpFile f{writeTmp("t3.json", std::string(10000, '{'))};

    PredictionConfig cfg;
    cfg.preferSpeed = true;
    PredictionEngine engine;
    engine.setConfig(cfg);
    const auto r = engine.predict(f.path);

    REQUIRE_FALSE(r.shouldSkip);
    REQUIRE(r.recommendations.size() >= 2);
    // First option must be at least as fast as the second
    CHECK(static_cast<int>(r.recommendations[0].speed) <=
          static_cast<int>(r.recommendations[1].speed));
}

// ---------------------------------------------------------------------------
// Progress callback
// ---------------------------------------------------------------------------

TEST_CASE("PredictionEngine invokes progress callback", "[prediction-engine]") {
    TmpFile f{writeTmp("t4.json", R"({"a":1})")};
    PredictionEngine engine;

    std::vector<std::string> stages;
    engine.predict(f.path, [&](const std::string& stage, double) {
        stages.push_back(stage);
    });

    REQUIRE_FALSE(stages.empty());
    const bool hasComplete =
        std::find(stages.begin(), stages.end(), "complete") != stages.end();
    CHECK(hasComplete);
}

// ---------------------------------------------------------------------------
// Batch prediction
// ---------------------------------------------------------------------------

TEST_CASE("PredictionEngine predictBatch", "[prediction-engine]") {
    TmpFile f1{writeTmp("b1.json", R"({"a":1})")};
    TmpFile f2{writeTmp("b2.cpp",  "int main(){}")};
    TmpFile f3{writeTmp("b3.txt",  "hello world")};

    PredictionEngine engine;
    int batchTicks = 0;
    const auto results = engine.predictBatch(
        {f1.path, f2.path, f3.path},
        [&](const std::string& stage, double) {
            if (stage == "batch") ++batchTicks;
        });

    REQUIRE(results.size() == 3);
    CHECK(results[0].extension == ".json");
    CHECK(results[1].extension == ".cpp");
    CHECK(results[2].extension == ".txt");
    CHECK(batchTicks >= 1);
}

// ---------------------------------------------------------------------------
// JSON serialisation
// ---------------------------------------------------------------------------

TEST_CASE("PredictionResult toJson produces valid JSON", "[prediction-engine]") {
    TmpFile f{writeTmp("ser.json", R"({"test":true})")};
    PredictionEngine engine;
    const auto r = engine.predict(f.path);
    const std::string json = r.toJson();

    CHECK_FALSE(json.empty());
    CHECK(json.find("\"file\"")            != std::string::npos);
    CHECK(json.find("\"extension\"")       != std::string::npos);
    CHECK(json.find("\"recommendations\"") != std::string::npos);
    CHECK(json.find("\"should_skip\"")     != std::string::npos);
    CHECK(json.find("\"preprocessor\"")    != std::string::npos);
}

TEST_CASE("PredictionResult fromJson roundtrip", "[prediction-engine]") {
    TmpFile f{writeTmp("rt.json", R"({"roundtrip":42})")};
    PredictionEngine engine;
    const auto original = engine.predict(f.path);
    const std::string json = original.toJson();

    const auto restored = PredictionResult::fromJson(json);
    CHECK(restored.extension          == original.extension);
    CHECK(restored.shouldSkip         == original.shouldSkip);
    CHECK(restored.preprocessor       == original.preprocessor);
    CHECK(restored.preprocessorAvailable == original.preprocessorAvailable);
    CHECK(restored.recommendations.size() == original.recommendations.size());

    if (!original.recommendations.empty()) {
        CHECK(restored.recommendations[0].algorithm == original.recommendations[0].algorithm);
        CHECK(restored.recommendations[0].level     == original.recommendations[0].level);
        CHECK(restored.recommendations[0].speedLabel == original.recommendations[0].speedLabel);
    }
}

TEST_CASE("PredictionResult toJson skip file", "[prediction-engine]") {
    TmpFile f{writeTmpJpeg("skip_ser.jpg")};
    PredictionEngine engine;
    const auto r = engine.predict(f.path);
    REQUIRE(r.shouldSkip);

    const std::string json = r.toJson();
    CHECK(json.find("\"should_skip\": true") != std::string::npos);
    CHECK(json.find("\"skip_reason\"")        != std::string::npos);
}

// ---------------------------------------------------------------------------
// Custom decision matrix path
// ---------------------------------------------------------------------------

TEST_CASE("PredictionEngine loads custom decision matrix path", "[prediction-engine]") {
#ifndef DECISION_MATRIX_JSON_PATH
    SKIP("DECISION_MATRIX_JSON_PATH not defined");
#else
    const std::string pathStr = DECISION_MATRIX_JSON_PATH;
    if (pathStr.empty()) SKIP("empty path");

    PredictionConfig cfg;
    cfg.decisionMatrixPath = pathStr;
    PredictionEngine engine;
    engine.setConfig(cfg);
    CHECK(engine.getStats().supportedExtensions == 34);
#endif
}
