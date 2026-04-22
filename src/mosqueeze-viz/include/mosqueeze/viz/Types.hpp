#pragma once

#include <chrono>
#include <filesystem>
#include <string>

namespace mosqueeze::viz {

struct BenchmarkRow {
    std::string algorithm;
    int level = 0;
    std::filesystem::path file;
    int fileType = 0;
    size_t originalBytes = 0;
    size_t compressedBytes = 0;
    std::chrono::milliseconds encodeTime{0};
    std::chrono::milliseconds decodeTime{0};
    size_t peakMemoryBytes = 0;

    double ratio() const {
        return compressedBytes > 0
            ? static_cast<double>(originalBytes) / static_cast<double>(compressedBytes)
            : 0.0;
    }
};

} // namespace mosqueeze::viz
