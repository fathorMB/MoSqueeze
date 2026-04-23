#pragma once

#include <mosqueeze/ICompressionEngine.hpp>
#include <mosqueeze/Preprocessor.hpp>
#include <mosqueeze/engines/BrotliEngine.hpp>
#include <mosqueeze/engines/LzmaEngine.hpp>
#include <mosqueeze/engines/ZpaqEngine.hpp>
#include <mosqueeze/engines/ZstdEngine.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mosqueeze::server {

struct CompressResult {
    bool success = false;
    std::vector<uint8_t> output;
    int64_t inputSize = 0;
    int64_t outputSize = 0;
    double ratio = 0.0;
    std::string algorithm;
    int level = 0;
    std::string preprocessorUsed = "none";
    std::string errorMessage;
};

struct DecompressResult {
    bool success = false;
    std::vector<uint8_t> output;
    int64_t inputSize = 0;
    int64_t outputSize = 0;
    std::string preprocessorUsed = "unknown";
    std::string algorithm = "unknown";
    std::string errorMessage;
};

class Compressor {
public:
    Compressor();

    CompressResult compress(
        const std::vector<uint8_t>& input,
        const std::string& algorithm,
        int level,
        const std::string& preprocessor);

    DecompressResult decompress(const std::vector<uint8_t>& input);

    static std::vector<std::string> supportedAlgorithms();
    static std::pair<int, int> algorithmLevelRange(const std::string& algorithm);
    static int defaultLevel(const std::string& algorithm);

private:
    ICompressionEngine* getEngine(const std::string& algorithm);
    static std::unique_ptr<IPreprocessor> createPreprocessor(const std::string& preprocessor);

    ZstdEngine zstd_;
    BrotliEngine brotli_;
    LzmaEngine lzma_;
    ZpaqEngine zpaq_;
};

} // namespace mosqueeze::server
