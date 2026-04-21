#include <mosqueeze/engines/LzmaEngine.hpp>

#include "platform/Platform.hpp"

#include <lzma.h>

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

std::string lzmaErrorToString(lzma_ret code) {
    switch (code) {
    case LZMA_OK:
        return "LZMA_OK";
    case LZMA_STREAM_END:
        return "LZMA_STREAM_END";
    case LZMA_NO_CHECK:
        return "LZMA_NO_CHECK";
    case LZMA_UNSUPPORTED_CHECK:
        return "LZMA_UNSUPPORTED_CHECK";
    case LZMA_GET_CHECK:
        return "LZMA_GET_CHECK";
    case LZMA_MEM_ERROR:
        return "LZMA_MEM_ERROR";
    case LZMA_MEMLIMIT_ERROR:
        return "LZMA_MEMLIMIT_ERROR";
    case LZMA_FORMAT_ERROR:
        return "LZMA_FORMAT_ERROR";
    case LZMA_OPTIONS_ERROR:
        return "LZMA_OPTIONS_ERROR";
    case LZMA_DATA_ERROR:
        return "LZMA_DATA_ERROR";
    case LZMA_BUF_ERROR:
        return "LZMA_BUF_ERROR";
    case LZMA_PROG_ERROR:
        return "LZMA_PROG_ERROR";
    default:
        return "LZMA_UNKNOWN_ERROR";
    }
}

} // namespace

std::vector<int> LzmaEngine::supportedLevels() const {
    std::vector<int> levels;
    levels.reserve(static_cast<size_t>(maxLevel()) + 1);
    for (int level = 0; level <= maxLevel(); ++level) {
        levels.push_back(level);
    }
    return levels;
}

CompressionResult LzmaEngine::compress(
    std::istream& input,
    std::ostream& output,
    const CompressionOptions& opts) {
    auto startedAt = std::chrono::steady_clock::now();

    int level = opts.level;
    if (level < 0 || level > maxLevel()) {
        level = defaultLevel();
    }

    uint32_t preset = static_cast<uint32_t>(level);
    if (opts.extreme) {
        preset |= LZMA_PRESET_EXTREME;
    }

    lzma_stream stream = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_easy_encoder(&stream, preset, LZMA_CHECK_CRC64);
    if (ret != LZMA_OK) {
        throw std::runtime_error("lzma_easy_encoder failed: " + lzmaErrorToString(ret));
    }

    struct StreamGuard {
        lzma_stream* stream;
        ~StreamGuard() { lzma_end(stream); }
    } guard{&stream};

    std::array<uint8_t, BUFFER_SIZE> inData{};
    std::array<uint8_t, BUFFER_SIZE> outData{};

    CompressionResult result{};
    bool inputFinished = false;

    while (true) {
        if (stream.avail_in == 0 && !inputFinished) {
            input.read(reinterpret_cast<char*>(inData.data()), static_cast<std::streamsize>(inData.size()));
            const auto readCount = static_cast<size_t>(input.gcount());
            result.originalBytes += readCount;

            stream.next_in = inData.data();
            stream.avail_in = readCount;

            inputFinished = input.eof();
            ensureStreamReadable(input, "lzma compression read");
        }

        stream.next_out = outData.data();
        stream.avail_out = outData.size();

        const lzma_action action = inputFinished ? LZMA_FINISH : LZMA_RUN;
        ret = lzma_code(&stream, action);

        if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
            throw std::runtime_error("lzma_code compress failed: " + lzmaErrorToString(ret));
        }

        const size_t produced = outData.size() - stream.avail_out;
        if (produced > 0) {
            output.write(reinterpret_cast<const char*>(outData.data()), static_cast<std::streamsize>(produced));
            ensureStreamWritable(output, "lzma compression write");
            result.compressedBytes += produced;
        }

        if (ret == LZMA_STREAM_END) {
            break;
        }
    }

    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startedAt);
    result.peakMemoryBytes = platform::getPeakMemoryUsage();
    return result;
}

void LzmaEngine::decompress(std::istream& input, std::ostream& output) {
    lzma_stream stream = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_stream_decoder(&stream, UINT64_MAX, 0);
    if (ret != LZMA_OK) {
        throw std::runtime_error("lzma_stream_decoder failed: " + lzmaErrorToString(ret));
    }

    struct StreamGuard {
        lzma_stream* stream;
        ~StreamGuard() { lzma_end(stream); }
    } guard{&stream};

    std::array<uint8_t, BUFFER_SIZE> inData{};
    std::array<uint8_t, BUFFER_SIZE> outData{};

    bool inputFinished = false;

    while (true) {
        if (stream.avail_in == 0 && !inputFinished) {
            input.read(reinterpret_cast<char*>(inData.data()), static_cast<std::streamsize>(inData.size()));
            const auto readCount = static_cast<size_t>(input.gcount());
            stream.next_in = inData.data();
            stream.avail_in = readCount;
            inputFinished = input.eof();
            ensureStreamReadable(input, "lzma decompression read");
        }

        stream.next_out = outData.data();
        stream.avail_out = outData.size();

        ret = lzma_code(&stream, LZMA_RUN);
        if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
            throw std::runtime_error("lzma_code decompress failed: " + lzmaErrorToString(ret));
        }

        const size_t produced = outData.size() - stream.avail_out;
        if (produced > 0) {
            output.write(reinterpret_cast<const char*>(outData.data()), static_cast<std::streamsize>(produced));
            ensureStreamWritable(output, "lzma decompression write");
        }

        if (ret == LZMA_STREAM_END) {
            break;
        }

        if (inputFinished && stream.avail_in == 0 && produced == 0) {
            throw std::runtime_error("Incomplete xz stream: unexpected end of input");
        }
    }
}

} // namespace mosqueeze
