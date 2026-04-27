#include "DecompressCommand.hpp"
#include "CompressCommand.hpp"
#include "../ProgressIndicator.hpp"

#include <mosqueeze/CompressionPipeline.hpp>
#include <mosqueeze/ICompressionEngine.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace mosqueeze::cli {
namespace {

constexpr int EXIT_ERROR = 1;
constexpr size_t PROGRESS_THRESHOLD = 1024 * 1024; // 1 MB

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

std::string deriveOutputPath(const std::string& input) {
    if (input == "-") {
        return "-";
    }

    std::filesystem::path path(input);
    if (path.extension() == ".msz") {
        path.replace_extension();
        return path.string();
    }
    return path.string() + ".out";
}

bool hasPrefix(const std::string& value, const std::vector<uint8_t>& magic) {
    return value.size() >= magic.size() &&
        std::equal(magic.begin(), magic.end(), reinterpret_cast<const uint8_t*>(value.data()));
}

std::vector<std::string> detectCandidateAlgorithms(
    const std::string& payload,
    const std::string& inputFile) {
    std::vector<std::string> candidates;

    // Magic-based first.
    if (hasPrefix(payload, {0x28, 0xB5, 0x2F, 0xFD})) {
        candidates.push_back("zstd");
    }
    if (hasPrefix(payload, {0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00})) {
        candidates.push_back("lzma");
    }
    // zpaq streams often start with "zPQ" marker.
    if (payload.size() >= 3 && payload[0] == 'z' && payload[1] == 'P' && payload[2] == 'Q') {
        candidates.push_back("zpaq");
    }

    // Extension hints second.
    if (inputFile != "-") {
        const std::string ext = std::filesystem::path(inputFile).extension().string();
        if (ext == ".zst" || ext == ".zstd") {
            candidates.push_back("zstd");
        } else if (ext == ".xz" || ext == ".lzma") {
            candidates.push_back("lzma");
        } else if (ext == ".br") {
            candidates.push_back("brotli");
        } else if (ext == ".zpaq") {
            candidates.push_back("zpaq");
        }
    }

    // Fallback order with brotli included.
    for (const std::string& algo : {"zstd", "lzma", "brotli", "zpaq"}) {
        candidates.push_back(algo);
    }

    std::vector<std::string> unique;
    unique.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        if (std::find(unique.begin(), unique.end(), candidate) == unique.end()) {
            unique.push_back(candidate);
        }
    }
    return unique;
}

std::pair<std::string, std::string> tryDecompressWithDetection(
    const std::string& compressedPayload,
    const std::string& inputFile) {
    std::string lastError;
    for (const auto& algorithm : detectCandidateAlgorithms(compressedPayload, inputFile)) {
        try {
            auto engine = createEngine(algorithm);
            CompressionPipeline pipeline(engine.get());
            std::istringstream in(compressedPayload);
            std::ostringstream out;
            pipeline.decompress(in, out);
            return {algorithm, out.str()};
        } catch (const std::exception& e) {
            lastError = e.what();
        }
    }
    throw std::runtime_error(
        "Failed to detect decompression algorithm for input. Last error: " + lastError);
}

} // namespace

void addDecompressCommand(CLI::App& app, const Terminal& term) {
    auto* decompress = app.add_subcommand("decompress", "Decompress a file");
    auto opts = std::make_shared<DecompressOptions>();

    decompress->add_option("input", opts->inputFile, "Input file (use - for stdin)")
        ->required();
    decompress->add_option("-o,--output", opts->outputFile, "Output file (default: input without .msz)");
    decompress->add_flag("--json", opts->jsonOutput, "Output result as JSON");

    decompress->footer(R"(
Examples:
  mosqueeze decompress output.msz
  mosqueeze decompress archive.msz -o restored.dat
  mosqueeze decompress compressed.msz -o - | other-command
  mosqueeze decompress input.msz --json

Note: Algorithm is auto-detected from the file content and extension.
)");

    decompress->callback([opts, &term]() {
        const int exitCode = runDecompress(*opts, term);
        if (exitCode != 0) {
            throw CLI::RuntimeError(exitCode);
        }
    });
}

int runDecompress(const DecompressOptions& opts, const Terminal& term) {
    try {
        const bool useInputStdIn = opts.inputFile == "-";
        const std::string resolvedOutput = opts.outputFile.empty() ? deriveOutputPath(opts.inputFile) : opts.outputFile;
        const bool useOutputStdOut = resolvedOutput == "-";
        setBinaryModeIfNeeded(useInputStdIn, useOutputStdOut);

        if (!useInputStdIn && !std::filesystem::exists(opts.inputFile)) {
            if (opts.jsonOutput) {
                emitErrorJson(opts.inputFile, "Input file not found");
            } else {
                term.printError("Input file not found: " + opts.inputFile);
            }
            return EXIT_ERROR;
        }

        // Get file size for progress indication
        size_t inputFileSize = 0;
        if (!useInputStdIn) {
            inputFileSize = std::filesystem::file_size(opts.inputFile);
        }

        // Show progress for large files
        ProgressIndicator progress(term);
        if (inputFileSize >= PROGRESS_THRESHOLD && !useOutputStdOut) {
            progress.showProcessing("Decompressing", opts.inputFile, inputFileSize);
        }

        std::ifstream inFile;
        std::istream* input = &std::cin;
        if (!useInputStdIn) {
            inFile.open(opts.inputFile, std::ios::binary);
            if (!inFile) {
                throw std::runtime_error("Failed to open input file: " + opts.inputFile);
            }
            input = &inFile;
        }

        const std::string compressedPayload{
            std::istreambuf_iterator<char>(*input),
            std::istreambuf_iterator<char>()};
        const size_t inputSize = compressedPayload.size();

        const auto [algorithm, decompressedPayload] =
            tryDecompressWithDetection(compressedPayload, opts.inputFile);
        const size_t outputSize = decompressedPayload.size();

        std::ofstream outFile;
        std::ostream* output = &std::cout;
        if (!useOutputStdOut) {
            std::filesystem::path outPath(resolvedOutput);
            if (outPath.has_parent_path()) {
                std::filesystem::create_directories(outPath.parent_path());
            }
            outFile.open(resolvedOutput, std::ios::binary);
            if (!outFile) {
                throw std::runtime_error("Failed to open output file: " + resolvedOutput);
            }
            output = &outFile;
        }

        output->write(decompressedPayload.data(), static_cast<std::streamsize>(decompressedPayload.size()));
        output->flush();

        progress.clearProcessing();

        const double ratio = inputSize > 0
            ? static_cast<double>(outputSize) / static_cast<double>(inputSize)
            : 0.0;
        const double elapsed = progress.elapsedSeconds();

        if (opts.jsonOutput) {
            nlohmann::json payload{
                {"success", true},
                {"input", opts.inputFile},
                {"output", resolvedOutput},
                {"inputSize", inputSize},
                {"outputSize", outputSize},
                {"ratio", ratio},
                {"algorithm", algorithm}
            };
            if (elapsed > 0.0) {
                payload["elapsedSeconds"] = elapsed;
            }
            fmt::print("{}\n", payload.dump());
        } else {
            term.printSuccess(fmt::format("Decompressed {} -> {}", opts.inputFile, resolvedOutput));
            fmt::print("  {}Algorithm:{} {}\n", term.bold(), term.reset(), algorithm);
            fmt::print("  {}Size:{} {} -> {} bytes ({:.1f}%)\n",
                       term.bold(), term.reset(),
                       inputSize,
                       outputSize,
                       inputSize > 0 ? (100.0 * static_cast<double>(outputSize) / static_cast<double>(inputSize)) : 0.0);
            if (elapsed > 0.0) {
                fmt::print("  {}Time:{} {:.2f}s\n", term.bold(), term.reset(), elapsed);
            }
        }

        return 0;
    } catch (const std::exception& e) {
        if (opts.jsonOutput) {
            emitErrorJson(opts.inputFile, e.what());
        } else {
            term.printError(e.what());
        }
        return EXIT_ERROR;
    }
}

} // namespace mosqueeze::cli