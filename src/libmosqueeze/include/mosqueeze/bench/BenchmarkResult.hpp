#pragma once

#include <mosqueeze/Types.hpp>

#include <chrono>
#include <filesystem>
#include <string>

namespace mosqueeze::bench {

struct BenchmarkResult {
    std::string algorithm;
    int level = 0;
    std::filesystem::path file;
    FileType fileType = FileType::Unknown;
    size_t originalBytes = 0;
    size_t compressedBytes = 0;
    std::chrono::milliseconds encodeTime{0};
    std::chrono::milliseconds decodeTime{0};
    size_t peakMemoryBytes = 0;

    double ratio() const {
        return compressedBytes > 0
            ? static_cast<double>(originalBytes) / compressedBytes
            : 0.0;
    }
};

} // namespace mosqueeze::bench
