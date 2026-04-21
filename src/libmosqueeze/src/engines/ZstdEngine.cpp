#include <mosqueeze/engines/ZstdEngine.hpp>

#include "platform/Platform.hpp"

#include <zstd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <stdexcept>
#include <string>

namespace mosqueeze {
namespace {

constexpr size_t BUFFER_SIZE = 64 * 1024;

void ensureStreamGood(const std::ios& stream, const char* context) {
    if (!stream.good() && !stream.eof()) {
        throw std::runtime_error(std::string("I/O stream error during ") + context);
    }
}

void ensureZstdOk(size_t code, const char* context) {
    if (ZSTD_isError(code) != 0) {
        throw std::runtime_error(
            std::string(context) + ": " + ZSTD_getErrorName(code));
    }
}

} // namespace

std::vector<int> ZstdEngine::supportedLevels() const {
    std::vector<int> levels;
    levels.reserve(static_cast<size_t>(maxLevel()));
    for (int level = 1; level <= maxLevel(); ++level) {
        levels.push_back(level);
    }
    return levels;
}

CompressionResult ZstdEngine::compress(
    std::istream& input,
    std::ostream& output,
    const CompressionOptions& opts) {
    auto startedAt = std::chrono::steady_clock::now();

    const int zstdMaxLevel = ZSTD_maxCLevel();
    int level = opts.level > 0 ? opts.level : defaultLevel();
    if (opts.extreme) {
        level = std::max(level, maxLevel());
    }
    level = std::clamp(level, 1, zstdMaxLevel);

    ZSTD_CStream* cstream = ZSTD_createCStream();
    if (cstream == nullptr) {
        throw std::runtime_error("Failed to create ZSTD_CStream");
    }

    struct StreamGuard {
        ZSTD_CStream* stream;
        ~StreamGuard() { ZSTD_freeCStream(stream); }
    } cguard{cstream};

    ensureZstdOk(ZSTD_initCStream(cstream, level), "ZSTD_initCStream failed");

    std::array<char, BUFFER_SIZE> inData{};
    std::array<char, BUFFER_SIZE> outData{};

    CompressionResult result{};

    while (true) {
        input.read(inData.data(), static_cast<std::streamsize>(inData.size()));
        const auto readCount = static_cast<size_t>(input.gcount());
        result.originalBytes += readCount;

        ZSTD_inBuffer inBuffer{inData.data(), readCount, 0};

        while (inBuffer.pos < inBuffer.size) {
            ZSTD_outBuffer outBuffer{outData.data(), outData.size(), 0};
            ensureZstdOk(
                ZSTD_compressStream2(cstream, &outBuffer, &inBuffer, ZSTD_e_continue),
                "ZSTD_compressStream2 failed");

            if (outBuffer.pos > 0) {
                output.write(outData.data(), static_cast<std::streamsize>(outBuffer.pos));
                ensureStreamGood(output, "compression write");
                result.compressedBytes += outBuffer.pos;
            }
        }

        if (input.eof()) {
            break;
        }

        ensureStreamGood(input, "compression read");
    }

    size_t remaining = 0;
    do {
        ZSTD_inBuffer emptyInput{inData.data(), 0, 0};
        ZSTD_outBuffer outBuffer{outData.data(), outData.size(), 0};
        remaining = ZSTD_compressStream2(cstream, &outBuffer, &emptyInput, ZSTD_e_end);
        ensureZstdOk(remaining, "ZSTD_compressStream2 finalization failed");

        if (outBuffer.pos > 0) {
            output.write(outData.data(), static_cast<std::streamsize>(outBuffer.pos));
            ensureStreamGood(output, "final compression write");
            result.compressedBytes += outBuffer.pos;
        }
    } while (remaining != 0);

    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startedAt);
    result.peakMemoryBytes = platform::getPeakMemoryUsage();
    return result;
}

void ZstdEngine::decompress(std::istream& input, std::ostream& output) {
    ZSTD_DStream* dstream = ZSTD_createDStream();
    if (dstream == nullptr) {
        throw std::runtime_error("Failed to create ZSTD_DStream");
    }

    struct StreamGuard {
        ZSTD_DStream* stream;
        ~StreamGuard() { ZSTD_freeDStream(stream); }
    } dguard{dstream};

    ensureZstdOk(ZSTD_initDStream(dstream), "ZSTD_initDStream failed");

    std::array<char, BUFFER_SIZE> inData{};
    std::array<char, BUFFER_SIZE> outData{};

    size_t lastResult = 0;

    while (true) {
        input.read(inData.data(), static_cast<std::streamsize>(inData.size()));
        const auto readCount = static_cast<size_t>(input.gcount());

        if (readCount == 0) {
            if (input.eof()) {
                break;
            }
            ensureStreamGood(input, "decompression read");
        }

        ZSTD_inBuffer inBuffer{inData.data(), readCount, 0};

        while (inBuffer.pos < inBuffer.size) {
            ZSTD_outBuffer outBuffer{outData.data(), outData.size(), 0};
            lastResult = ZSTD_decompressStream(dstream, &outBuffer, &inBuffer);
            ensureZstdOk(lastResult, "ZSTD_decompressStream failed");

            if (outBuffer.pos > 0) {
                output.write(outData.data(), static_cast<std::streamsize>(outBuffer.pos));
                ensureStreamGood(output, "decompression write");
            }
        }

        if (input.eof()) {
            break;
        }
    }

    if (lastResult != 0) {
        throw std::runtime_error("Incomplete zstd stream: unexpected end of input");
    }
}

} // namespace mosqueeze
