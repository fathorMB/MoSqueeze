#include "compressor.hpp"

#include <mosqueeze/CompressionPipeline.hpp>
#include <mosqueeze/preprocessors/BayerPreprocessor.hpp>
#include <mosqueeze/preprocessors/ImageMetaStripper.hpp>
#include <mosqueeze/preprocessors/JsonCanonicalizer.hpp>
#include <mosqueeze/preprocessors/PngOptimizer.hpp>
#include <mosqueeze/preprocessors/XmlCanonicalizer.hpp>

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace mosqueeze::server {
namespace {

bool hasPrefix(const std::vector<uint8_t>& payload, std::initializer_list<uint8_t> prefix) {
    return payload.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), payload.begin());
}

std::vector<std::string> detectAlgorithms(const std::vector<uint8_t>& payload) {
    std::vector<std::string> candidates;
    if (hasPrefix(payload, {0x28, 0xB5, 0x2F, 0xFD})) {
        candidates.push_back("zstd");
    }
    if (hasPrefix(payload, {0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00})) {
        candidates.push_back("lzma");
    }
    if (payload.size() >= 3 && payload[0] == 'z' && payload[1] == 'P' && payload[2] == 'Q') {
        candidates.push_back("zpaq");
    }

    for (const std::string& algo : {"zstd", "lzma", "brotli", "zpaq"}) {
        if (std::find(candidates.begin(), candidates.end(), algo) == candidates.end()) {
            candidates.push_back(algo);
        }
    }
    return candidates;
}

} // namespace

Compressor::Compressor() = default;

CompressResult Compressor::compress(
    const std::vector<uint8_t>& input,
    const std::string& algorithm,
    int level,
    const std::string& preprocessor) {
    CompressResult result{};
    result.inputSize = static_cast<int64_t>(input.size());
    result.algorithm = algorithm;
    result.level = level > 0 ? level : defaultLevel(algorithm);
    result.preprocessorUsed = preprocessor.empty() ? "none" : preprocessor;

    try {
        ICompressionEngine* engine = getEngine(algorithm);
        if (engine == nullptr) {
            result.errorMessage = "Unknown algorithm: " + algorithm;
            return result;
        }

        const auto supported = engine->supportedLevels();
        if (std::find(supported.begin(), supported.end(), result.level) == supported.end()) {
            result.errorMessage = "Invalid level for algorithm: " + algorithm;
            return result;
        }

        CompressionPipeline pipeline(engine);
        std::unique_ptr<IPreprocessor> preprocessorOwned = createPreprocessor(result.preprocessorUsed);
        if (preprocessorOwned) {
            pipeline.setPreprocessor(preprocessorOwned.get());
        } else {
            result.preprocessorUsed = "none";
        }

        std::istringstream inputStream(std::string(input.begin(), input.end()), std::ios::binary);
        std::ostringstream outputStream(std::ios::binary);

        CompressionOptions options{};
        options.level = result.level;
        const PipelineResult pipe = pipeline.compress(inputStream, outputStream, options);

        const std::string compressed = outputStream.str();
        result.output.assign(compressed.begin(), compressed.end());
        result.outputSize = static_cast<int64_t>(result.output.size());
        result.ratio = result.inputSize > 0
            ? static_cast<double>(result.outputSize) / static_cast<double>(result.inputSize)
            : 0.0;
        result.success = true;
    } catch (const std::exception& e) {
        result.errorMessage = e.what();
    }

    return result;
}

DecompressResult Compressor::decompress(const std::vector<uint8_t>& input) {
    DecompressResult result{};
    result.inputSize = static_cast<int64_t>(input.size());

    for (const auto& algorithm : detectAlgorithms(input)) {
        try {
            ICompressionEngine* engine = getEngine(algorithm);
            if (engine == nullptr) {
                continue;
            }

            CompressionPipeline pipeline(engine);
            std::istringstream inputStream(std::string(input.begin(), input.end()), std::ios::binary);
            std::ostringstream outputStream(std::ios::binary);
            pipeline.decompress(inputStream, outputStream);

            const std::string decompressed = outputStream.str();
            result.output.assign(decompressed.begin(), decompressed.end());
            result.outputSize = static_cast<int64_t>(result.output.size());
            result.success = true;
            result.algorithm = algorithm;
            return result;
        } catch (const std::exception& e) {
            result.errorMessage = e.what();
        }
    }

    if (result.errorMessage.empty()) {
        result.errorMessage = "Unable to detect a supported compression algorithm";
    }
    return result;
}

std::vector<std::string> Compressor::supportedAlgorithms() {
    return {"zstd", "brotli", "lzma", "zpaq"};
}

std::pair<int, int> Compressor::algorithmLevelRange(const std::string& algorithm) {
    if (algorithm == "zstd") {
        return {1, 22};
    }
    if (algorithm == "brotli") {
        return {0, 11};
    }
    if (algorithm == "lzma") {
        return {0, 9};
    }
    if (algorithm == "zpaq") {
        return {1, 5};
    }
    return {0, 0};
}

int Compressor::defaultLevel(const std::string& algorithm) {
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

ICompressionEngine* Compressor::getEngine(const std::string& algorithm) {
    if (algorithm == "zstd") {
        return &zstd_;
    }
    if (algorithm == "brotli") {
        return &brotli_;
    }
    if (algorithm == "lzma") {
        return &lzma_;
    }
    if (algorithm == "zpaq") {
        return &zpaq_;
    }
    return nullptr;
}

std::unique_ptr<IPreprocessor> Compressor::createPreprocessor(const std::string& preprocessor) {
    if (preprocessor == "none" || preprocessor.empty()) {
        return nullptr;
    }
    if (preprocessor == "json-canonical") {
        return std::make_unique<JsonCanonicalizer>();
    }
    if (preprocessor == "xml-canonical") {
        return std::make_unique<XmlCanonicalizer>();
    }
    if (preprocessor == "image-meta-strip") {
        return std::make_unique<ImageMetaStripper>();
    }
    if (preprocessor == "png-optimizer") {
        return std::make_unique<PngOptimizer>();
    }
    if (preprocessor == "bayer-raw") {
        return std::make_unique<BayerPreprocessor>();
    }
    return nullptr;
}

} // namespace mosqueeze::server
