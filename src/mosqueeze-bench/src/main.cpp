#include <mosqueeze/bench/BenchmarkRunner.hpp>
#include <mosqueeze/bench/CorpusManager.hpp>
#include <mosqueeze/bench/ResultsStore.hpp>
#include <mosqueeze/engines/BrotliEngine.hpp>
#include <mosqueeze/engines/LzmaEngine.hpp>
#include <mosqueeze/engines/ZstdEngine.hpp>

#include <CLI/CLI.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    CLI::App app{"MoSqueeze Benchmark Harness", "mosqueeze-bench"};

    std::string corpusPath = "benchmarks/corpus";
    std::string outputPath = "benchmarks/results";
    bool json = true;
    bool csv = false;

    app.add_option("-c,--corpus", corpusPath, "Path to corpus directory");
    app.add_option("-o,--output", outputPath, "Output directory for results");
    app.add_flag("--json", json, "Export results as JSON");
    app.add_flag("--csv", csv, "Export results as CSV");

    CLI11_PARSE(app, argc, argv);

    mosqueeze::bench::BenchmarkRunner runner;
    runner.registerEngine(std::make_unique<mosqueeze::ZstdEngine>());
    runner.registerEngine(std::make_unique<mosqueeze::LzmaEngine>());
    runner.registerEngine(std::make_unique<mosqueeze::BrotliEngine>());

    mosqueeze::bench::CorpusManager corpus(corpusPath);
    corpus.downloadStandardCorpus();

    std::filesystem::create_directories(outputPath);
    mosqueeze::bench::ResultsStore store((std::filesystem::path(outputPath) / "results.sqlite3"));

    std::cout << "MoSqueeze Benchmark Harness v0.1.0\n";
    std::cout << "Corpus: " << corpus.totalFiles() << " files ("
              << corpus.totalSize() << " bytes)\n";

    auto files = corpus.listFiles();
    std::vector<std::filesystem::path> paths;
    paths.reserve(files.size());
    for (const auto& file : files) {
        paths.push_back(file.path);
    }

    auto results = runner.runGrid(paths);
    store.clear();
    store.saveAll(results);

    if (json) {
        const auto jsonPath = std::filesystem::path(outputPath) / "results.json";
        store.exportJson(jsonPath);
        std::cout << "Exported JSON to " << jsonPath.string() << "\n";
    }
    if (csv) {
        const auto csvPath = std::filesystem::path(outputPath) / "results.csv";
        store.exportCsv(csvPath);
        std::cout << "Exported CSV to " << csvPath.string() << "\n";
    }

    std::cout << "\n=== Top 5 by Ratio ===\n";
    std::sort(results.begin(), results.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.ratio() > rhs.ratio();
    });

    const size_t topN = std::min<size_t>(results.size(), 5);
    for (size_t i = 0; i < topN; ++i) {
        const auto& r = results[i];
        std::cout << r.algorithm << "/" << r.level << ": "
                  << r.ratio() << "x (" << r.file.filename().string() << ")\n";
    }

    return 0;
}
