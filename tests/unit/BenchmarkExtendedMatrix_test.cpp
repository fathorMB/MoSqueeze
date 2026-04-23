#include <mosqueeze/bench/BenchmarkRunner.hpp>
#include <mosqueeze/bench/ResultsStore.hpp>
#include <mosqueeze/engines/ZstdEngine.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>

int main() {
    namespace fs = std::filesystem;
    using mosqueeze::bench::BenchmarkConfig;
    using mosqueeze::bench::BenchmarkRunner;
    using mosqueeze::bench::ResultsStore;

    const fs::path tempDir = fs::temp_directory_path() / "mosqueeze_benchmark_extended_matrix_test";
    fs::create_directories(tempDir);
    const fs::path jsonFile = tempDir / "sample.json";
    const fs::path dbPath = tempDir / "results.sqlite3";

    {
        std::ofstream out(jsonFile, std::ios::binary);
        out << "{\"z\":1,\"a\":2,\"items\":[1,2,3,4]}";
    }

    BenchmarkRunner runner;
    runner.registerEngine(std::make_unique<mosqueeze::ZstdEngine>());

    BenchmarkConfig config;
    config.files = {jsonFile};
    config.algorithms = {"zstd"};
    config.defaultOnly = true;
    config.iterations = 1;
    config.warmupIterations = 0;
    config.runDecode = false;
    config.extendedMatrix = true;
    config.preprocessAll = true;

    const auto firstRun = runner.runWithConfig(config);
    bool sawNone = false;
    bool sawJsonCanonical = false;
    for (const auto& row : firstRun) {
        if (row.preprocess.type == "none") {
            sawNone = true;
        }
        if (row.preprocess.type == "json-canonical") {
            sawJsonCanonical = true;
        }
    }
    assert(sawNone);
    assert(sawJsonCanonical);

    std::unordered_set<std::string> existing;
    {
        ResultsStore store(dbPath);
        store.clear();
        store.saveAll(firstRun);
        existing = store.loadExistingKeys();
        assert(!existing.empty());
    }

    BenchmarkConfig resumeConfig = config;
    resumeConfig.skipExistingKeys = existing;
    const auto resumedRun = runner.runWithConfig(resumeConfig);
    assert(resumedRun.empty());

    fs::remove_all(tempDir);
    std::cout << "[PASS] BenchmarkExtendedMatrix_test\n";
    return 0;
}
