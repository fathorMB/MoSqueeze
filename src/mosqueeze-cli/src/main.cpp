#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <mosqueeze/AlgorithmSelector.hpp>
#include <mosqueeze/FileTypeDetector.hpp>
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

namespace {

void addAnalyzeCommand(CLI::App& app) {
    auto* analyze = app.add_subcommand("analyze", "Analyze file and recommend algorithm");

    auto inputFile = std::make_shared<std::string>();
    auto verbose = std::make_shared<bool>(false);
    auto benchmark = std::make_shared<bool>(false);

    analyze->add_option("file", *inputFile, "File to analyze")->required();
    analyze->add_flag("-v,--verbose", *verbose, "Show detailed analysis");
    analyze->add_flag("-b,--benchmark", *benchmark, "Run quick benchmark");

    analyze->callback([inputFile, verbose, benchmark]() {
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

        if (*benchmark) {
            fmt::print("\nQuick benchmark not implemented yet; use mosqueeze-bench for full runs.\n");
        }
    });
}

} // namespace

int main(int argc, char** argv) {
    CLI::App app{"MoSqueeze - Cold Storage Compression Library"};

    app.add_flag("-V,--version", [](std::int64_t) {
        fmt::print("mosqueeze v{}\n", mosqueeze::versionString());
        throw CLI::Success();
    }, "Print version and exit");

    addAnalyzeCommand(app);

    CLI11_PARSE(app, argc, argv);

    return 0;
}
