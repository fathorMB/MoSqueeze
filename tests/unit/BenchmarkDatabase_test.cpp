#include <mosqueeze/BenchmarkDatabase.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>

int main() {
    const auto dbPath = std::filesystem::temp_directory_path() / "mosqueeze-benchmark-db-test.sqlite3";
    std::error_code removeEc;
    std::filesystem::remove(dbPath, removeEc);

    mosqueeze::BenchmarkDatabase db;
    assert(db.open(dbPath));
    assert(db.isOpen());

    mosqueeze::FileFeatures features{};
    features.detectedType = "application/json";
    features.extension = ".json";
    features.fileSize = 4096;
    features.entropy = 4.2;
    features.isStructured = true;

    const bool recorded = db.recordResult(
        "json-1",
        features,
        "json-canonical",
        "brotli",
        11,
        512,
        4096,
        std::chrono::milliseconds(35));
    assert(recorded);

    const auto results = db.query(features, mosqueeze::OptimizationGoal::MinSize, 5);
    assert(!results.empty());
    assert(results.front().algorithm == "brotli");
    assert(results.front().preprocessor == "json-canonical");
    assert(results.front().expectedRatio > 0.0);
    assert(results.front().sampleCount >= 1);

    db.close();
    std::filesystem::remove(dbPath, removeEc);

    std::cout << "[PASS] BenchmarkDatabase_test\n";
    return 0;
}
