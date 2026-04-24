#include <catch2/catch_test_macros.hpp>
#include <mosqueeze/DecisionMatrix.hpp>

#include <algorithm>
#include <string>

using namespace mosqueeze;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static DecisionMatrix loadedMatrix() {
    DecisionMatrix dm;
    REQUIRE(dm.loadDefault());
    return dm;
}

// ---------------------------------------------------------------------------
// Load & size
// ---------------------------------------------------------------------------

TEST_CASE("DecisionMatrix loads default embedded data", "[decision-matrix]") {
    DecisionMatrix dm;
    REQUIRE(dm.loadDefault());
    REQUIRE(dm.totalEntries() == 34);
    REQUIRE(dm.totalBenchmarks() > 0);
    REQUIRE(dm.knownExtensions().size() == 34);
}

TEST_CASE("DecisionMatrix loads from file", "[decision-matrix]") {
#ifndef DECISION_MATRIX_JSON_PATH
    SKIP("DECISION_MATRIX_JSON_PATH not defined");
#else
    const std::string path = DECISION_MATRIX_JSON_PATH;
    if (path.empty()) SKIP("DECISION_MATRIX_JSON_PATH is empty");

    DecisionMatrix dm;
    REQUIRE(dm.loadFromFile(path));
    REQUIRE(dm.totalEntries() == 34);
#endif
}

// ---------------------------------------------------------------------------
// hasData / knownExtensions
// ---------------------------------------------------------------------------

TEST_CASE("DecisionMatrix hasData", "[decision-matrix]") {
    const auto dm = loadedMatrix();

    CHECK(dm.hasData(".json"));
    CHECK(dm.hasData("json"));   // without leading dot
    CHECK(dm.hasData(".JSON"));  // case-insensitive
    CHECK_FALSE(dm.hasData(".unknown_xyz_qq"));
}

TEST_CASE("DecisionMatrix knownExtensions is sorted", "[decision-matrix]") {
    const auto dm = loadedMatrix();
    const auto exts = dm.knownExtensions();
    REQUIRE(!exts.empty());
    CHECK(std::is_sorted(exts.begin(), exts.end()));
}

// ---------------------------------------------------------------------------
// Predict — compressible extensions
// ---------------------------------------------------------------------------

TEST_CASE("DecisionMatrix predicts .json", "[decision-matrix]") {
    const auto dm = loadedMatrix();
    const auto pred = dm.predict(".json");

    CHECK_FALSE(pred.shouldSkip);
    CHECK(pred.fileExtension == ".json");
    CHECK(pred.fileType == FileType::Text_Structured);
    REQUIRE_FALSE(pred.options.empty());

    // best_ratio_algo for .json is xz/9 → normalised to lzma
    CHECK(pred.options[0].algorithm == "lzma");
    CHECK(pred.options[0].level == 9);
    CHECK(pred.options[0].estimatedRatio > 1.0);
    CHECK(pred.getBest() != nullptr);
}

TEST_CASE("DecisionMatrix predicts .cpp", "[decision-matrix]") {
    const auto dm = loadedMatrix();
    const auto pred = dm.predict(".cpp");

    CHECK_FALSE(pred.shouldSkip);
    REQUIRE_FALSE(pred.options.empty());
    // .cpp has best_ratio_algo zpaq/5
    CHECK(pred.options[0].algorithm == "zpaq");
    CHECK(pred.options[0].level == 5);
    // has fast and balanced options
    CHECK(pred.options.size() >= 2);
}

TEST_CASE("DecisionMatrix returns all three option tiers when available", "[decision-matrix]") {
    // .json has best_ratio, best_fast, and best_balanced
    const auto dm = loadedMatrix();
    const auto pred = dm.predict(".json");
    REQUIRE(pred.options.size() == 3);
    CHECK(pred.getBest()     != nullptr);
    CHECK(pred.getFastest()  != nullptr);
    CHECK(pred.getBalanced() != nullptr);
}

// ---------------------------------------------------------------------------
// Predict — skip extensions
// ---------------------------------------------------------------------------

TEST_CASE("DecisionMatrix skips .mp4", "[decision-matrix]") {
    const auto dm = loadedMatrix();
    const auto pred = dm.predict(".mp4");

    CHECK(pred.shouldSkip);
    CHECK_FALSE(pred.skipReason.empty());
    CHECK(pred.options.empty());
    // skipReason mentions the extension
    CHECK(pred.skipReason.find(".mp4") != std::string::npos);
}

TEST_CASE("DecisionMatrix skips .jpg", "[decision-matrix]") {
    const auto dm = loadedMatrix();
    const auto pred = dm.predict(".jpg");
    CHECK(pred.shouldSkip);
}

TEST_CASE("DecisionMatrix skips all expected extensions", "[decision-matrix]") {
    const auto dm = loadedMatrix();
    // These are the 7 extensions with avg_ratio < 1.1 and no fast/balanced option
    for (const char* ext : {".7z", ".avi", ".gz", ".jpg", ".mov", ".mp4", ".png"}) {
        const auto pred = dm.predict(ext);
        INFO("extension: " << ext);
        CHECK(pred.shouldSkip);
    }
}

// ---------------------------------------------------------------------------
// Predict — unknown extension fallback
// ---------------------------------------------------------------------------

TEST_CASE("DecisionMatrix fallback for unknown extension", "[decision-matrix]") {
    const auto dm = loadedMatrix();
    const auto pred = dm.predict(".unknown_xyz_qq");

    CHECK_FALSE(pred.shouldSkip);
    REQUIRE_FALSE(pred.options.empty());
    CHECK(pred.options[0].algorithm == "zstd");
    CHECK(pred.options[0].level == 22);
}

TEST_CASE("DecisionMatrix normalises extension without leading dot", "[decision-matrix]") {
    const auto dm = loadedMatrix();
    const auto pred_with    = dm.predict(".json");
    const auto pred_without = dm.predict("json");
    CHECK(pred_with.shouldSkip  == pred_without.shouldSkip);
    CHECK(pred_with.options.size() == pred_without.options.size());
    if (!pred_with.options.empty() && !pred_without.options.empty())
        CHECK(pred_with.options[0].algorithm == pred_without.options[0].algorithm);
}

// ---------------------------------------------------------------------------
// Speed labels
// ---------------------------------------------------------------------------

TEST_CASE("DecisionMatrix speed labels are valid", "[decision-matrix]") {
    const auto dm = loadedMatrix();
    const auto pred = dm.predict(".json");
    REQUIRE_FALSE(pred.options.empty());
    for (const auto& opt : pred.options) {
        INFO("speedLabel: " << opt.speedLabel);
        CHECK((opt.speedLabel == "fast" ||
               opt.speedLabel == "medium" ||
               opt.speedLabel == "slow"));
    }
}

TEST_CASE("DecisionMatrix speed enum matches label", "[decision-matrix]") {
    const auto dm = loadedMatrix();
    const auto pred = dm.predict(".json");
    for (const auto& opt : pred.options) {
        if (opt.speed == CompressionSpeed::Fast)
            CHECK(opt.speedLabel == "fast");
        else if (opt.speed == CompressionSpeed::Medium)
            CHECK(opt.speedLabel == "medium");
        else
            CHECK(opt.speedLabel == "slow");
    }
}

// ---------------------------------------------------------------------------
// File size estimation
// ---------------------------------------------------------------------------

TEST_CASE("DecisionMatrix estimates compressed size", "[decision-matrix]") {
    const auto dm = loadedMatrix();
    constexpr size_t kInputBytes = 1'000'000; // 1 MB
    const auto pred = dm.predict(".json", kInputBytes);

    REQUIRE_FALSE(pred.options.empty());
    const auto& best = pred.options[0];
    CHECK(best.estimatedRatio > 1.0);
    CHECK(best.estimatedSize > 0);
    CHECK(best.estimatedSize < kInputBytes);
    // estimatedSize ≈ inputSize / ratio  (within 1 byte rounding)
    const auto expected = static_cast<size_t>(kInputBytes / best.estimatedRatio);
    CHECK(best.estimatedSize == expected);
}

TEST_CASE("DecisionMatrix zero fileSize yields zero estimatedSize", "[decision-matrix]") {
    const auto dm = loadedMatrix();
    const auto pred = dm.predict(".json", 0);
    REQUIRE_FALSE(pred.options.empty());
    // No file size given → can't estimate
    CHECK(pred.options[0].estimatedSize == 0);
}

// ---------------------------------------------------------------------------
// Fallback fields
// ---------------------------------------------------------------------------

TEST_CASE("DecisionMatrix sets fallback on best option", "[decision-matrix]") {
    const auto dm = loadedMatrix();
    // .cpp has fast+balanced → best option must have a fallback
    const auto pred = dm.predict(".cpp");
    REQUIRE_FALSE(pred.options.empty());
    CHECK_FALSE(pred.options[0].fallbackAlgorithm.empty());
    CHECK(pred.options[0].fallbackLevel > 0);
}

// ---------------------------------------------------------------------------
// predict(path) overload
// ---------------------------------------------------------------------------

TEST_CASE("DecisionMatrix predict(path) extracts extension", "[decision-matrix]") {
    const auto dm = loadedMatrix();
    const std::filesystem::path p = "archive.json";
    const auto pred = dm.predict(p);
    CHECK(pred.fileExtension == ".json");
    CHECK_FALSE(pred.shouldSkip);
}
