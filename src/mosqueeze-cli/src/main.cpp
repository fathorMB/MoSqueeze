#include "commands/CompressCommand.hpp"
#include "commands/DecompressCommand.hpp"
#include "commands/PredictCommand.hpp"
#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <mosqueeze/AlgorithmSelector.hpp>
#include <mosqueeze/IntelligentSelector.hpp>
#include <mosqueeze/FileTypeDetector.hpp>
#include <mosqueeze/PreprocessorSelector.hpp>
#include <mosqueeze/Version.hpp>
#include <mosqueeze/engines/BrotliEngine.hpp>
#include <mosqueeze/engines/LzmaEngine.hpp>
#include <mosqueeze/engines/ZpaqEngine.hpp>
#include <mosqueeze/engines/ZstdEngine.hpp>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

void addAnalyzeCommand(CLI::App& app) {
    auto* analyze = app.add_subcommand("analyze", "Analyze file and recommend algorithm");

    auto inputFile = std::make_shared<std::string>();
    auto verbose = std::make_shared<bool>(false);
    auto benchmark = std::make_shared<bool>(false);
    auto preprocess = std::make_shared<std::string>("none");

    analyze->add_option("file", *inputFile, "File to analyze")->required();
    analyze->add_flag("-v,--verbose", *verbose, "Show detailed analysis");
    analyze->add_flag("-b,--benchmark", *benchmark, "Run quick benchmark");
    analyze
        ->add_option(
            "--preprocess",
            *preprocess,
            "Preprocessor: auto, none, json-canonical, xml-canonical, image-meta-strip, png-optimizer, bayer-raw, zstd-dict")
        ->check(CLI::IsMember(
            {"auto", "none", "json-canonical", "xml-canonical", "image-meta-strip", "png-optimizer", "bayer-raw", "zstd-dict"}));

    analyze->callback([inputFile, verbose, benchmark, preprocess]() {
        std::filesystem::path path(*inputFile);
        if (!std::filesystem::exists(path)) {
            throw std::runtime_error("Input file does not exist: " + path.string());
        }

        mosqueeze::FileTypeDetector detector;
        const auto classification = detector.detect(path);

        mosqueeze::AlgorithmSelector selector;
        const mosqueeze::ZstdEngine zstd;
        const mosqueeze::LzmaEngine lzma;
        const mosqueeze::BrotliEngine brotli;
        const mosqueeze::ZpaqEngine zpaq;
        selector.registerEngine(&zstd);
        selector.registerEngine(&lzma);
        selector.registerEngine(&brotli);
        selector.registerEngine(&zpaq);

        const auto selection = selector.select(classification, path);

        fmt::print("File: {}\n", path.string());
        fmt::print("Type: {}\n", classification.mimeType);
        fmt::print("Compressed: {}\n", classification.isCompressed ? "yes" : "no");

        if (*verbose) {
            fmt::print("FileType enum: {}\n", static_cast<int>(classification.type));
            fmt::print("Can recompress: {}\n", classification.canRecompress ? "yes" : "no");
            auto algorithms = selector.availableAlgorithms();
            fmt::print("Available algorithms: {}\n", fmt::join(algorithms, ", "));
        }

        fmt::print("\nRecommendation:\n");

        if (selection.shouldSkip) {
            fmt::print("  Action: SKIP\n");
            fmt::print("  Reason: {}\n", selection.rationale);
            return;
        }

        fmt::print("  Algorithm: {} level {}\n", selection.algorithm, selection.level);
        if (!selection.fallbackAlgorithm.empty()) {
            fmt::print("  Fallback: {} level {}\n", selection.fallbackAlgorithm, selection.fallbackLevel);
        }
        fmt::print("  Reason: {}\n", selection.rationale);
        fmt::print("  Preprocess: {}\n", *preprocess);

        if (*benchmark) {
            fmt::print("\nQuick benchmark not implemented yet; use mosqueeze-bench for full runs.\n");
        }
    });
}

mosqueeze::OptimizationGoal parseGoal(const std::string& value) {
    if (value == "fastest") {
        return mosqueeze::OptimizationGoal::Fastest;
    }
    if (value == "balanced") {
        return mosqueeze::OptimizationGoal::Balanced;
    }
    if (value == "min-memory") {
        return mosqueeze::OptimizationGoal::MinMemory;
    }
    if (value == "best-decompression") {
        return mosqueeze::OptimizationGoal::BestDecompression;
    }
    return mosqueeze::OptimizationGoal::MinSize;
}

void printRecommendation(const mosqueeze::Recommendation& rec, bool jsonOutput) {
    if (jsonOutput) {
        fmt::print(
            "{{\"preprocessor\":\"{}\",\"algorithm\":\"{}\",\"level\":{},\"expectedRatio\":{},\"expectedTimeMs\":{},\"expectedSize\":{},\"confidence\":{},\"sampleCount\":{},\"explanation\":\"{}\"}}\n",
            rec.preprocessor,
            rec.algorithm,
            rec.level,
            rec.expectedRatio,
            rec.expectedTimeMs,
            rec.expectedSize,
            rec.confidence,
            rec.sampleCount,
            rec.explanation);
        return;
    }

    fmt::print("Recommendation\n");
    fmt::print("  Preprocessor: {}\n", rec.preprocessor);
    fmt::print("  Algorithm:    {}\n", rec.algorithm);
    fmt::print("  Level:        {}\n", rec.level);
    fmt::print("  Expected ratio: {:.3f}\n", rec.expectedRatio);
    fmt::print("  Expected time : {:.1f} ms\n", rec.expectedTimeMs);
    fmt::print("  Confidence    : {:.0f}%\n", rec.confidence * 100.0);
    fmt::print("  Explanation   : {}\n", rec.explanation);

    if (!rec.alternatives.empty()) {
        fmt::print("Alternatives:\n");
        for (const auto& alt : rec.alternatives) {
            fmt::print(
                "  - {} + {}:{} (ratio {:.3f}, {:.1f} ms, conf {:.0f}%)\n",
                alt.preprocessor,
                alt.algorithm,
                alt.level,
                alt.expectedRatio,
                alt.expectedTimeMs,
                alt.confidence * 100.0);
        }
    }
}

void addSuggestCommand(CLI::App& app) {
    auto* suggest = app.add_subcommand("suggest", "Intelligent compression suggestion for a single file");

    auto inputFile = std::make_shared<std::string>();
    auto goal = std::make_shared<std::string>("min-size");
    auto dbPath = std::make_shared<std::string>();
    auto jsonOutput = std::make_shared<bool>(false);

    suggest->add_option("file", *inputFile, "File to analyze")->required();
    suggest
        ->add_option("--goal", *goal, "Optimization goal: min-size, fastest, balanced, min-memory, best-decompression")
        ->check(CLI::IsMember({"min-size", "fastest", "balanced", "min-memory", "best-decompression"}))
        ->default_val("min-size");
    suggest->add_option("--db", *dbPath, "Optional benchmark database path");
    suggest->add_flag("--json", *jsonOutput, "Print compact JSON output");

    suggest->callback([inputFile, goal, dbPath, jsonOutput]() {
        std::filesystem::path path(*inputFile);
        if (!std::filesystem::exists(path)) {
            throw std::runtime_error("Input file does not exist: " + path.string());
        }

        mosqueeze::IntelligentSelector selector(parseGoal(*goal));
        if (!dbPath->empty()) {
            const bool opened = selector.loadBenchmarkDatabase(*dbPath);
            if (!opened) {
                throw std::runtime_error("Failed to open benchmark database: " + *dbPath);
            }
        }

        auto recommendation = selector.analyzeWithAlternatives(
            path,
            {mosqueeze::OptimizationGoal::Fastest, mosqueeze::OptimizationGoal::Balanced});
        printRecommendation(recommendation, *jsonOutput);
    });
}

} // namespace

int main(int argc, char** argv) {
    CLI::App app{"MoSqueeze - Cold Storage Compression Library"};
    bool listPreprocessors = false;

    app.add_flag("-V,--version", [](std::int64_t) {
        fmt::print("mosqueeze v{}\n", mosqueeze::versionString());
        throw CLI::Success();
    }, "Print version and exit");
    app.add_flag("--list-preprocessors", listPreprocessors, "List available preprocessors");

    addAnalyzeCommand(app);
    addSuggestCommand(app);
    mosqueeze::cli::addCompressCommand(app);
    mosqueeze::cli::addDecompressCommand(app);
    mosqueeze::cli::addPredictCommand(app);

    CLI11_PARSE(app, argc, argv);

    if (listPreprocessors) {
        mosqueeze::PreprocessorSelector selector;
        fmt::print("Available preprocessors: {}\n", fmt::join(selector.listNames(), ", "));
    }

    return 0;
}
