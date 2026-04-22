#include <mosqueeze/bench/BenchmarkRunner.hpp>
#include <mosqueeze/bench/ResultsStore.hpp>
#include <mosqueeze/engines/ZstdEngine.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main() {
    namespace fs = std::filesystem;
    using mosqueeze::bench::BenchmarkConfig;
    using mosqueeze::bench::BenchmarkRunner;
    using mosqueeze::bench::ResultsStore;

    const fs::path tempDir = fs::temp_directory_path() / "mosqueeze_benchmark_preprocess_test";
    fs::create_directories(tempDir);
    const fs::path inputPath = tempDir / "sample.json";
    const fs::path jsonOut = tempDir / "results.json";

    {
        std::ofstream out(inputPath, std::ios::binary);
        out << "{\n  \"b\": 1,\n  \"a\": 2,\n  \"nested\": {\"z\": 1, \"y\": 2}\n}\n";
    }

    BenchmarkRunner runner;
    runner.registerEngine(std::make_unique<mosqueeze::ZstdEngine>());

    BenchmarkConfig config;
    config.files = {inputPath};
    config.algorithms = {"zstd"};
    config.defaultOnly = true;
    config.iterations = 1;
    config.warmupIterations = 0;
    config.runDecode = false;

    config.preprocessMode = "none";
    const auto withoutPreprocess = runner.runWithConfig(config);
    assert(withoutPreprocess.size() == 1);
    assert(!withoutPreprocess[0].preprocess.applied);

    config.preprocessMode = "json-canonical";
    const auto withPreprocess = runner.runWithConfig(config);
    assert(withPreprocess.size() == 1);
    assert(withPreprocess[0].preprocess.applied);
    assert(withPreprocess[0].preprocess.type == "json-canonical");
    assert(withPreprocess[0].preprocess.originalBytes > 0);
    assert(withPreprocess[0].preprocess.processedBytes > 0);

    ResultsStore store(":memory:");
    store.saveAll(withPreprocess);
    const auto stored = store.query();
    assert(stored.size() == 1);
    assert(stored[0].preprocess.applied);
    assert(stored[0].preprocess.type == "json-canonical");

    store.exportJson(jsonOut);
    {
        std::ifstream in(jsonOut, std::ios::binary);
        const std::string payload((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        assert(payload.find("\"preprocess\"") != std::string::npos);
        assert(payload.find("\"totalRatio\"") != std::string::npos);
        assert(payload.find("\"json-canonical\"") != std::string::npos);
    }

    fs::remove_all(tempDir);
    std::cout << "[PASS] BenchmarkPreprocess_test\n";
    return 0;
}
