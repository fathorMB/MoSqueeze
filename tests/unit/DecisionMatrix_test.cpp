#include <mosqueeze/DecisionMatrix.hpp>

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

using namespace mosqueeze;

#ifndef DECISION_MATRIX_JSON_PATH
#define DECISION_MATRIX_JSON_PATH ""
#endif

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static DecisionMatrix loadMatrix() {
    DecisionMatrix m;
    // Try embedded resource first, then fall back to the source-tree path.
    if (!m.loadDefault()) {
        const std::filesystem::path p = DECISION_MATRIX_JSON_PATH;
        assert(!p.empty() && "No embedded data and DECISION_MATRIX_JSON_PATH not set");
        assert(m.loadFromFile(p) && "loadFromFile failed");
    }
    return m;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void testLoadAndSize() {
    const auto m = loadMatrix();
    assert(m.totalEntries() > 0 && "decision matrix should have entries");
    assert(m.totalBenchmarks() > 0 && "total benchmarks should be > 0");
    assert(m.knownExtensions().size() == m.totalEntries());
    std::cout << "[PASS] load: " << m.totalEntries() << " extensions, "
              << m.totalBenchmarks() << " benchmarks\n";
}

void testHasData() {
    const auto m = loadMatrix();
    assert(m.hasData(".json"));
    assert(m.hasData("json"));   // without leading dot
    assert(m.hasData(".JSON"));  // case-insensitive
    assert(!m.hasData(".unknown_xyz_qq"));
    std::cout << "[PASS] hasData\n";
}

void testPredictJson() {
    const auto m = loadMatrix();
    const auto p = m.predict(".json");
    assert(!p.shouldSkip);
    assert(p.fileExtension == ".json");
    assert(p.fileType == FileType::Text_Structured);
    assert(!p.options.empty());
    assert(p.getBest() != nullptr);
    assert(p.getBest()->estimatedRatio > 1.0);
    assert(!p.getBest()->algorithm.empty());
    assert(p.getBest()->level > 0);
    std::cout << "[PASS] predict .json: best=" << p.getBest()->algorithm
              << "/" << p.getBest()->level
              << " ratio=" << p.getBest()->estimatedRatio << "x\n";
}

void testSkipFiles() {
    const auto m = loadMatrix();

    for (const auto& ext : {".jpg", ".mp4", ".7z", ".gz", ".mov", ".png"}) {
        if (!m.hasData(ext)) continue; // skip if not in this build's matrix
        const auto p = m.predict(ext);
        assert(p.shouldSkip && (std::string("expected shouldSkip for ") + ext).c_str());
        assert(!p.skipReason.empty());
        assert(p.options.empty() && "skip predictions must have no options");
        std::cout << "[PASS] skip " << ext << ": " << p.skipReason << "\n";
    }
}

void testUnknownFallback() {
    const auto m = loadMatrix();
    const auto p = m.predict(".unknown_xyz_qq");
    assert(!p.shouldSkip);
    assert(!p.options.empty());
    assert(p.getBest()->algorithm == "zstd");
    assert(p.getBest()->level == 22);
    std::cout << "[PASS] unknown extension fallback → zstd/22\n";
}

void testSpeedLabels() {
    const auto m = loadMatrix();
    // .cpp has both fast and balanced entries with known speed tiers
    const auto p = m.predict(".cpp");
    assert(!p.shouldSkip);
    for (const auto& opt : p.options) {
        assert(!opt.speedLabel.empty());
        assert(opt.speedLabel == "fast" || opt.speedLabel == "medium" || opt.speedLabel == "slow");
    }
    std::cout << "[PASS] speed labels valid for .cpp\n";
}

void testEstimatedSize() {
    const auto m = loadMatrix();
    constexpr size_t kInputBytes = 100'000;
    const auto p = m.predict(".json", kInputBytes);
    assert(!p.shouldSkip);
    const auto* best = p.getBest();
    assert(best != nullptr);
    // estimatedSize should be input / ratio
    if (best->estimatedRatio > 1.0) {
        assert(best->estimatedSize > 0);
        assert(best->estimatedSize < kInputBytes);
    }
    std::cout << "[PASS] estimated size for 100KB .json → "
              << best->estimatedSize << " bytes\n";
}

void testPredictFromPath() {
    const auto m = loadMatrix();
    const std::filesystem::path p = "sample_archive.json";
    const auto pred = m.predict(p);
    assert(!pred.shouldSkip);
    assert(pred.fileExtension == ".json");
    std::cout << "[PASS] predict(path) extracts extension correctly\n";
}

void testPredictAllKnownExtensions() {
    const auto m = loadMatrix();
    size_t skipped = 0, predicted = 0;
    for (const auto& ext : m.knownExtensions()) {
        const auto p = m.predict(ext);
        assert(p.fileExtension == ext);
        if (p.shouldSkip) {
            ++skipped;
            assert(!p.skipReason.empty());
        } else {
            ++predicted;
            assert(!p.options.empty());
            assert(p.getBest() != nullptr);
            assert(!p.getBest()->algorithm.empty());
        }
    }
    assert(skipped + predicted == m.totalEntries());
    std::cout << "[PASS] all " << m.totalEntries() << " extensions: "
              << predicted << " predicted, " << skipped << " skipped\n";
}

void testFallbackFields() {
    const auto m = loadMatrix();
    // .cpp has fast + balanced → best option should have non-empty fallback
    const auto p = m.predict(".cpp");
    assert(!p.shouldSkip && !p.options.empty());
    const auto* best = p.getBest();
    assert(best != nullptr);
    assert(!best->fallbackAlgorithm.empty());
    std::cout << "[PASS] fallback fields set on best option\n";
}

void testLoadFromFileExplicit() {
    const std::filesystem::path path = DECISION_MATRIX_JSON_PATH;
    if (path.empty() || !std::filesystem::exists(path)) {
        std::cout << "[SKIP] loadFromFile (DECISION_MATRIX_JSON_PATH not set)\n";
        return;
    }
    DecisionMatrix m;
    assert(m.loadFromFile(path));
    assert(m.totalEntries() > 0);
    std::cout << "[PASS] loadFromFile explicit path\n";
}

// ---------------------------------------------------------------------------

int main() {
    testLoadAndSize();
    testHasData();
    testPredictJson();
    testSkipFiles();
    testUnknownFallback();
    testSpeedLabels();
    testEstimatedSize();
    testPredictFromPath();
    testPredictAllKnownExtensions();
    testFallbackFields();
    testLoadFromFileExplicit();

    std::cout << "[PASS] All DecisionMatrix tests passed\n";
    return 0;
}
