#include "CompressCommand.hpp"

#include <mosqueeze/CompressionPipeline.hpp>
#include <mosqueeze/Preprocessor.hpp>
#include <mosqueeze/engines/BrotliEngine.hpp>
#include <mosqueeze/engines/LzmaEngine.hpp>
#include <mosqueeze/engines/ZpaqEngine.hpp>
#include <mosqueeze/engines/ZstdEngine.hpp>
#include <mosqueeze/preprocessors/BayerPreprocessor.hpp>
#include <mosqueeze/preprocessors/ImageMetaStripper.hpp>
#include <mosqueeze/preprocessors/JsonCanonicalizer.hpp>
#include <mosqueeze/preprocessors/PngOptimizer.hpp>
#include <mosqueeze/preprocessors/XmlCanonicalizer.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace mosqueeze::cli {
namespace {

void setBinaryModeIfNeeded(bool useInputStdIn, bool useOutputStdOut) {
#ifdef _WIN32
    if (useInputStdIn) {
        _setmode(_fileno(stdin), _O_BINARY);
    }
    if (useOutputStdOut) {
        _setmode(_fileno(stdout), _O_BINARY);
    }
#else
    (void)useInputStdIn;
    (void)useOutputStdOut;
#endif
}

void emitErrorJson(const std::string& input, const std::string& message) {
    nlohmann::json payload{
        {"success", false},
        {"error", message},
        {"input", input}
    };
    fmt::print("{}\n", payload.dump());
}

} // namespace

void addCompressCommand(CLI::App& app) {
    auto* compress = app.add_subcommand("compress", "Compress a file");

    auto opts = std::make_shared<CompressOptions>();

    compress->add_option("input", opts->inputFile, "Input file (use - for stdin)")
        ->required();
    compress->add_option("-o,--output", opts->outputFile, "Output file (use - for stdout)")
        ->required();
    compress->add_option("-a,--algorithm", opts->algorithm, "Algorithm: zstd, brotli, lzma, zpaq")
        ->default_val("zstd")
        ->check(CLI::IsMember({"zstd", "brotli", "lzma", "zpaq"}));
    compress->add_option("-l,--level", opts->level, "Compression level (default: algo-specific)");
    compress->add_option("--preprocess", opts->preprocess, "Preprocessor type")
        ->default_val("none")
        ->check(CLI::IsMember({"none", "json-canonical", "xml-canonical", "image-meta-strip", "png-optimizer", "bayer-raw"}));
    compress->add_flag("--json", opts->jsonOutput, "Output result as JSON");

    compress->callback([opts]() {
        const int exitCode = runCompress(*opts);
        if (exitCode != 0) {
            throw CLI::RuntimeError(exitCode);
        }
    });
}

std::unique_ptr<ICompressionEngine> createEngine(const std::string& algorithm) {
    if (algorithm == "zstd") {
        return std::make_unique<ZstdEngine>();
    }
    if (algorithm == "brotli") {
        return std::make_unique<BrotliEngine>();
    }
    if (algorithm == "lzma") {
        return std::make_unique<LzmaEngine>();
    }
    if (algorithm == "zpaq") {
        return std::make_unique<ZpaqEngine>();
    }
    throw std::runtime_error("Unsupported algorithm: " + algorithm);
}

std::unique_ptr<IPreprocessor> createPreprocessor(const std::string& preprocess) {
    if (preprocess == "none") {
        return nullptr;
    }
    if (preprocess == "json-canonical") {
        return std::make_unique<JsonCanonicalizer>();
    }
    if (preprocess == "xml-canonical") {
        return std::make_unique<XmlCanonicalizer>();
    }
    if (preprocess == "image-meta-strip") {
        return std::make_unique<ImageMetaStripper>();
    }
    if (preprocess == "png-optimizer") {
        return std::make_unique<PngOptimizer>();
    }
    if (preprocess == "bayer-raw") {
        return std::make_unique<BayerPreprocessor>();
    }
    throw std::runtime_error("Unsupported preprocessor: " + preprocess);
}

int defaultLevelFor(const std::string& algorithm) {
    if (algorithm == "zstd") {
        return 3;
    }
    if (algorithm == "brotli") {
        return 6;
    }
    if (algorithm == "lzma") {
        return 6;
    }
    if (algorithm == "zpaq") {
        return 3;
    }
    return 3;
}

int runCompress(const CompressOptions& opts) {
    try {
        const bool useInputStdIn = opts.inputFile == "-";
        const bool useOutputStdOut = opts.outputFile == "-";
        setBinaryModeIfNeeded(useInputStdIn, useOutputStdOut);

        if (!useInputStdIn && !std::filesystem::exists(opts.inputFile)) {
            if (opts.jsonOutput) {
                emitErrorJson(opts.inputFile, "Input file not found");
            } else {
                fmt::print(stderr, "Error: Input file not found: {}\n", opts.inputFile);
            }
            return 1;
        }

        auto engine = createEngine(opts.algorithm);
        const auto supportedLevels = engine->supportedLevels();
        const int level = opts.level >= 0 ? opts.level : defaultLevelFor(opts.algorithm);
        if (std::find(supportedLevels.begin(), supportedLevels.end(), level) == supportedLevels.end()) {
            throw std::runtime_error(
                "Invalid level " + std::to_string(level) + " for algorithm " + opts.algorithm);
        }

        auto preprocessor = createPreprocessor(opts.preprocess);
        CompressionPipeline pipeline(engine.get());
        if (preprocessor) {
            pipeline.setPreprocessor(preprocessor.get());
        }

        std::ifstream inFile;
        std::ofstream outFile;
        std::istream* input = &std::cin;
        std::ostream* output = &std::cout;

        if (!useInputStdIn) {
            inFile.open(opts.inputFile, std::ios::binary);
            if (!inFile) {
                throw std::runtime_error("Failed to open input file: " + opts.inputFile);
            }
            input = &inFile;
        }

        if (!useOutputStdOut) {
            std::filesystem::path outPath(opts.outputFile);
            if (outPath.has_parent_path()) {
                std::filesystem::create_directories(outPath.parent_path());
            }
            outFile.open(opts.outputFile, std::ios::binary);
            if (!outFile) {
                throw std::runtime_error("Failed to open output file: " + opts.outputFile);
            }
            output = &outFile;
        }

        CompressionOptions compOpts{};
        compOpts.level = level;
        const PipelineResult pipelineResult = pipeline.compress(*input, *output, compOpts);
        output->flush();

        const size_t inputSize = pipelineResult.wasPreprocessed
            ? pipelineResult.preprocessing.originalBytes
            : pipelineResult.compression.originalBytes;
        const size_t outputSize = pipelineResult.compression.compressedBytes +
            static_cast<size_t>(9 + pipelineResult.preprocessing.metadata.size());
        const double ratio = inputSize > 0
            ? static_cast<double>(outputSize) / static_cast<double>(inputSize)
            : 0.0;

        if (opts.jsonOutput) {
            nlohmann::json payload{
                {"success", true},
                {"input", opts.inputFile},
                {"output", opts.outputFile},
                {"inputSize", inputSize},
                {"outputSize", outputSize},
                {"ratio", ratio},
                {"algorithm", opts.algorithm},
                {"level", level},
                {"preprocessor", opts.preprocess}
            };
            fmt::print("{}\n", payload.dump());
        } else {
            fmt::print("Compressed {} -> {}\n", opts.inputFile, opts.outputFile);
            fmt::print("  Algorithm: {} level {}\n", opts.algorithm, level);
            fmt::print("  Preprocess: {}\n", opts.preprocess);
            fmt::print("  Size: {} -> {} bytes ({:.1f}%)\n",
                       inputSize,
                       outputSize,
                       inputSize > 0 ? (100.0 * static_cast<double>(outputSize) / static_cast<double>(inputSize)) : 0.0);
        }

        return 0;
    } catch (const std::exception& e) {
        if (opts.jsonOutput) {
            emitErrorJson(opts.inputFile, e.what());
        } else {
            fmt::print(stderr, "Error: {}\n", e.what());
        }
        return 2;
    }
}

} // namespace mosqueeze::cli
