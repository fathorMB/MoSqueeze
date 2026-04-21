#include <mosqueeze/engines/ZpaqEngine.hpp>

#include "platform/Platform.hpp"

#include <libzpaq.h>

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>

namespace libzpaq {

void error(const char* msg) {
    throw std::runtime_error(msg != nullptr ? msg : "libzpaq error");
}

} // namespace libzpaq

namespace mosqueeze {
namespace {

class StreamReader : public libzpaq::Reader {
public:
    explicit StreamReader(std::istream& stream)
        : stream_(stream) {}

    int get() override {
        const auto value = stream_.get();
        if (value == std::char_traits<char>::eof()) {
            return -1;
        }

        ++pos_;
        return value;
    }

    int read(char* buf, int n) override {
        stream_.read(buf, n);
        const int got = static_cast<int>(stream_.gcount());
        pos_ += static_cast<size_t>(got);
        return got;
    }

    size_t tell() const { return pos_; }

private:
    std::istream& stream_;
    size_t pos_ = 0;
};

class StreamWriter : public libzpaq::Writer {
public:
    explicit StreamWriter(std::ostream& stream)
        : stream_(stream) {}

    void put(int c) override {
        stream_.put(static_cast<char>(c));
        if (!stream_.good()) {
            throw std::runtime_error("I/O stream error during zpaq write");
        }
        ++pos_;
    }

    void write(const char* buf, int n) override {
        stream_.write(buf, n);
        if (!stream_.good()) {
            throw std::runtime_error("I/O stream error during zpaq write");
        }
        pos_ += static_cast<size_t>(n);
    }

    size_t tell() const { return pos_; }

private:
    std::ostream& stream_;
    size_t pos_ = 0;
};

int levelToMethod(int level, bool extreme) {
    int method = level > 0 ? level : 5;
    method = std::clamp(method, 1, 5);
    if (extreme) {
        method = 5;
    }
    return method;
}

} // namespace

struct ZpaqEngine::Impl {};

ZpaqEngine::ZpaqEngine()
    : impl_(std::make_unique<Impl>()) {}

ZpaqEngine::~ZpaqEngine() = default;

std::vector<int> ZpaqEngine::supportedLevels() const {
    return {1, 2, 3, 4, 5};
}

CompressionResult ZpaqEngine::compress(
    std::istream& input,
    std::ostream& output,
    const CompressionOptions& opts) {
    const auto startedAt = std::chrono::steady_clock::now();

    StreamReader reader(input);
    StreamWriter writer(output);

    const int method = levelToMethod(opts.level, opts.extreme);
    const std::string methodString = std::to_string(method);

    try {
        libzpaq::compress(&reader, &writer, methodString.c_str());
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("ZPAQ compression failed: ") + e.what());
    }

    CompressionResult result{};
    result.originalBytes = reader.tell();
    result.compressedBytes = writer.tell();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startedAt);
    result.peakMemoryBytes = platform::getPeakMemoryUsage();

    return result;
}

void ZpaqEngine::decompress(std::istream& input, std::ostream& output) {
    StreamReader reader(input);
    StreamWriter writer(output);

    try {
        libzpaq::decompress(&reader, &writer);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("ZPAQ decompression failed: ") + e.what());
    }
}

} // namespace mosqueeze
