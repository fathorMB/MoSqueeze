#include <mosqueeze/engines/BrotliEngine.hpp>

#include "platform/Platform.hpp"

#include <brotli/decode.h>
#include <brotli/encode.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <stdexcept>
#include <string>

namespace mosqueeze {
namespace {

constexpr size_t BUFFER_SIZE = 64 * 1024;

void ensureStreamReadable(const std::istream& input, const char* context) {
    if (!input.good() && !input.eof()) {
        throw std::runtime_error(std::string("I/O stream error during ") + context);
    }
}

void ensureStreamWritable(const std::ostream& output, const char* context) {
    if (!output.good()) {
        throw std::runtime_error(std::string("I/O stream error during ") + context);
    }
}

} // namespace

std::vector<int> BrotliEngine::supportedLevels() const {
    std::vector<int> levels;
    levels.reserve(static_cast<size_t>(maxLevel()) + 1);
    for (int level = 0; level <= maxLevel(); ++level) {
        levels.push_back(level);
    }
    return levels;
}

CompressionResult BrotliEngine::compress(
    std::istream& input,
    std::ostream& output,
    const CompressionOptions& opts) {
    auto startedAt = std::chrono::steady_clock::now();

    int quality = opts.level;
    if (quality < 0 || quality > maxLevel()) {
        quality = defaultLevel();
    }
    if (opts.extreme) {
        quality = maxLevel();
    }

    BrotliEncoderState* state = BrotliEncoderCreateInstance(nullptr, nullptr, nullptr);
    if (state == nullptr) {
        throw std::runtime_error("BrotliEncoderCreateInstance failed");
    }

    struct EncoderGuard {
        BrotliEncoderState* state;
        ~EncoderGuard() { BrotliEncoderDestroyInstance(state); }
    } guard{state};

    if (BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, static_cast<uint32_t>(quality)) == BROTLI_FALSE) {
        throw std::runtime_error("BrotliEncoderSetParameter(BROTLI_PARAM_QUALITY) failed");
    }

    std::array<uint8_t, BUFFER_SIZE> inData{};
    std::array<uint8_t, BUFFER_SIZE> outData{};

    CompressionResult result{};
    bool inputFinished = false;

    size_t availIn = 0;
    const uint8_t* nextIn = nullptr;

    while (true) {
        if (availIn == 0 && !inputFinished) {
            input.read(reinterpret_cast<char*>(inData.data()), static_cast<std::streamsize>(inData.size()));
            const auto readCount = static_cast<size_t>(input.gcount());
            result.originalBytes += readCount;

            availIn = readCount;
            nextIn = inData.data();

            inputFinished = input.eof();
            ensureStreamReadable(input, "brotli compression read");
        }

        BrotliEncoderOperation op = inputFinished ? BROTLI_OPERATION_FINISH : BROTLI_OPERATION_PROCESS;

        while (availIn > 0 || BrotliEncoderHasMoreOutput(state) == BROTLI_TRUE || op == BROTLI_OPERATION_FINISH) {
            size_t availOut = outData.size();
            uint8_t* nextOut = outData.data();
            size_t totalOut = 0;

            if (BrotliEncoderCompressStream(state, op, &availIn, &nextIn, &availOut, &nextOut, &totalOut) == BROTLI_FALSE) {
                throw std::runtime_error("BrotliEncoderCompressStream failed");
            }

            const size_t produced = outData.size() - availOut;
            if (produced > 0) {
                output.write(reinterpret_cast<const char*>(outData.data()), static_cast<std::streamsize>(produced));
                ensureStreamWritable(output, "brotli compression write");
                result.compressedBytes += produced;
            }

            if (op == BROTLI_OPERATION_FINISH && BrotliEncoderIsFinished(state) == BROTLI_TRUE) {
                result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startedAt);
                result.peakMemoryBytes = platform::getPeakMemoryUsage();
                return result;
            }

            if (op != BROTLI_OPERATION_FINISH && availIn == 0 && BrotliEncoderHasMoreOutput(state) == BROTLI_FALSE) {
                break;
            }

            if (op == BROTLI_OPERATION_FINISH &&
                availIn == 0 &&
                BrotliEncoderHasMoreOutput(state) == BROTLI_FALSE &&
                BrotliEncoderIsFinished(state) == BROTLI_FALSE &&
                produced == 0) {
                throw std::runtime_error("Brotli encoder did not finish stream");
            }
        }
    }
}

void BrotliEngine::decompress(std::istream& input, std::ostream& output) {
    BrotliDecoderState* state = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
    if (state == nullptr) {
        throw std::runtime_error("BrotliDecoderCreateInstance failed");
    }

    struct DecoderGuard {
        BrotliDecoderState* state;
        ~DecoderGuard() { BrotliDecoderDestroyInstance(state); }
    } guard{state};

    std::array<uint8_t, BUFFER_SIZE> inData{};
    std::array<uint8_t, BUFFER_SIZE> outData{};

    size_t availIn = 0;
    const uint8_t* nextIn = nullptr;
    bool inputFinished = false;

    while (true) {
        if (availIn == 0 && !inputFinished) {
            input.read(reinterpret_cast<char*>(inData.data()), static_cast<std::streamsize>(inData.size()));
            const auto readCount = static_cast<size_t>(input.gcount());
            availIn = readCount;
            nextIn = inData.data();
            inputFinished = input.eof();
            ensureStreamReadable(input, "brotli decompression read");
        }

        size_t availOut = outData.size();
        uint8_t* nextOut = outData.data();
        size_t totalOut = 0;

        const BrotliDecoderResult result =
            BrotliDecoderDecompressStream(state, &availIn, &nextIn, &availOut, &nextOut, &totalOut);

        const size_t produced = outData.size() - availOut;
        if (produced > 0) {
            output.write(reinterpret_cast<const char*>(outData.data()), static_cast<std::streamsize>(produced));
            ensureStreamWritable(output, "brotli decompression write");
        }

        if (result == BROTLI_DECODER_RESULT_SUCCESS) {
            return;
        }

        if (result == BROTLI_DECODER_RESULT_ERROR) {
            throw std::runtime_error("BrotliDecoderDecompressStream failed");
        }

        if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT && inputFinished && availIn == 0) {
            throw std::runtime_error("Incomplete brotli stream: unexpected end of input");
        }
    }
}

} // namespace mosqueeze
