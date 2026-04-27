#include "commands/CompressCommand.hpp"
#include "commands/DecompressCommand.hpp"
#include "commands/PredictCommand.hpp"
#include "Terminal.hpp"
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

#include <atomic>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

// Exit codes
constexpr int EXIT_SUCCESS_CODE = 0;
constexpr int EXIT_ERROR = 1;
constexpr int EXIT_CANCELLED = 2;
constexpr int EXIT_INVALID_ARGS = 3;

std::atomic<bool> g_interrupted{false};

void signalHandler(int) {
    g_interrupted = true;
}

void setupSignalHandlers() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
}

void addAnalyzeCommand(CLI::App& app, const mosqueeze::cli::Terminal& term) {
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

    analyze->footer(R"(
Examples:
  mosqueeze analyze document.json
  mosqueeze analyze photo.raf -v
  mosqueeze analyze data.csv --preprocess auto

Note: For benchmarking compression algorithms across multiple files,
use the separate mosqueeze-bench tool:

  mosqueeze-bench -d ./corpus -a zstd,xz,brotli,zpaq --default-only
)");

    analyze->callback([inputFile, verbose, benchmark, preprocess, &term]() {
        std::filesystem::path path(*inputFile);
        if (!std::filesystem::exists(path)) {
            term.printError("Input file does not exist: " + path.string());
            throw CLI::RuntimeError(EXIT_ERROR);
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

        fmt::print("{}File:{} {}\n", term.bold(), term.reset(), path.string());
        fmt::print("{}Type:{} {}\n", term.bold(), term.reset(), classification.mimeType);
        fmt::print("{}Compressed:{} {}\n", term.bold(), term.reset(), classification.isCompressed ? "yes" : "no");

        if (*verbose) {
            fmt::print("{}FileType enum:{} {}\n", term.bold(), term.reset(), static_cast<int>(classification.type));
            fmt::print("{}Can recompress:{} {}\n", term.bold(), term.reset(), classification.canRecompress ? "yes" : "no");
            auto algorithms = selector.availableAlgorithms();
            fmt::print("{}Available algorithms:{} {}\n", term.bold(), term.reset(), fmt::join(algorithms, ", "));
        }

        fmt::print("\n{}Recommendation:{}\n", term.bold(), term.reset());

        if (selection.shouldSkip) {
            fmt::print("  {}Action:{} SKIP\n", term.bold(), term.reset());
            fmt::print("  {}Reason:{} {}\n", term.bold(), term.reset(), selection.rationale);
            return;
        }

        term.printSuccess(fmt::format("Algorithm: {} level {}", selection.algorithm, selection.level));
        if (!selection.fallbackAlgorithm.empty()) {
            fmt::print("  {}Fallback:{} {} level {}\n", term.bold(), term.reset(), selection.fallbackAlgorithm, selection.fallbackLevel);
        }
        fmt::print("  {}Reason:{} {}\n", term.bold(), term.reset(), selection.rationale);
        fmt::print("  {}Preprocess:{} {}\n", term.bold(), term.reset(), *preprocess);

        if (*benchmark) {
            term.printWarning("Quick benchmark not implemented yet; use mosqueeze-bench for full runs.");
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

void printRecommendation(const mosqueeze::Recommendation& rec, bool jsonOutput, const mosqueeze::cli::Terminal& term) {
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

    fmt::print("{}Recommendation{}\n", term.bold(), term.reset());
    fmt::print("  {}Preprocessor:{} {}\n", term.bold(), term.reset(), rec.preprocessor);
    fmt::print("  {}Algorithm:{}    {}\n", term.bold(), term.reset(), rec.algorithm);
    fmt::print("  {}Level:{}        {}\n", term.bold(), term.reset(), rec.level);
    fmt::print("  {}Expected ratio:{} {:.3f}\n", term.bold(), term.reset(), rec.expectedRatio);
    fmt::print("  {}Expected time :{} {:.1f} ms\n", term.bold(), term.reset(), rec.expectedTimeMs);
    fmt::print("  {}Confidence    :{} {:.0f}%\n", term.bold(), term.reset(), rec.confidence * 100.0);
    fmt::print("  {}Explanation   :{} {}\n", term.bold(), term.reset(), rec.explanation);

    if (!rec.alternatives.empty()) {
        fmt::print("{}Alternatives:{}\n", term.bold(), term.reset());
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

void addSuggestCommand(CLI::App& app, const mosqueeze::cli::Terminal& term) {
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

    suggest->footer(R"(
Examples:
  mosqueeze suggest large.json
  mosqueeze suggest photo.raf --goal fastest
  mosqueeze suggest data.csv --goal balanced --db benchmarks.db
  mosqueeze suggest document.pdf --json

Goals:
  min-size           - Smallest output (default)
  fastest            - Fastest compression
  balanced           - Balance speed and ratio
  min-memory         - Minimal memory usage
  best-decompression - Fastest decompression
)");

    suggest->callback([inputFile, goal, dbPath, jsonOutput, &term]() {
        std::filesystem::path path(*inputFile);
        if (!std::filesystem::exists(path)) {
            term.printError("Input file does not exist: " + path.string());
            throw CLI::RuntimeError(EXIT_ERROR);
        }

        mosqueeze::IntelligentSelector selector(parseGoal(*goal));
        if (!dbPath->empty()) {
            const bool opened = selector.loadBenchmarkDatabase(*dbPath);
            if (!opened) {
                term.printError("Failed to open benchmark database: " + *dbPath);
                throw CLI::RuntimeError(EXIT_ERROR);
            }
        }

        auto recommendation = selector.analyzeWithAlternatives(
            path,
            {mosqueeze::OptimizationGoal::Fastest, mosqueeze::OptimizationGoal::Balanced});
        printRecommendation(recommendation, *jsonOutput, term);
    });
}

} // namespace

int main(int argc, char** argv) {
    setupSignalHandlers();

    mosqueeze::cli::Terminal term;

    CLI::App app{"MoSqueeze - Cold Storage Compression Library"};
    app.footer(R"(
Note: For benchmarking compression algorithms across multiple files,
use the separate mosqueeze-bench tool:

  mosqueeze-bench -d ./corpus -a zstd,xz,brotli,zpaq --default-only
)");

    bool listPreprocessors = false;

    app.add_flag("-V,--version", [](std::int64_t) {
        fmt::print("mosqueeze v{}\n", mosqueeze::versionString());
        throw CLI::Success();
    }, "Print version and exit");
    app.add_flag("--list-preprocessors", listPreprocessors, "List available preprocessors");

    addAnalyzeCommand(app, term);
    addSuggestCommand(app, term);
    mosqueeze::cli::addCompressCommand(app, term);
    mosqueeze::cli::addDecompressCommand(app, term);
    mosqueeze::cli::addPredictCommand(app, term);

    try {
        app.parse(argc, argv);
    } catch (const CLI::RuntimeError& e) {
        // RuntimeError is thrown by command callbacks with specific exit codes
        // (e.g., EXIT_ERROR=1 for file-not-found, etc.)
        return e.get_exit_code();
    } catch (const CLI::ParseError& e) {
        // CLI::Success (from --help, --version) returns 0
        if (e.get_exit_code() == 0) {
            return app.exit(e);
        }
        // All other parse errors = invalid arguments -> exit code 3
        return EXIT_INVALID_ARGS;
    }

    if (g_interrupted.load()) {
        term.printError("Operation cancelled by user");
        return EXIT_CANCELLED;
    }

    if (listPreprocessors) {
        mosqueeze::PreprocessorSelector selector;
        fmt::print("Available preprocessors: {}\n", fmt::join(selector.listNames(), ", "));
    }

    return EXIT_SUCCESS_CODE;
}