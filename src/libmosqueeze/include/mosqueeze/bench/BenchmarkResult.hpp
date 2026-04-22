#pragma once

#include <mosqueeze/Types.hpp>

#include <chrono>
#include <filesystem>
#include <string>

namespace mosqueeze::bench {

struct PreprocessMetrics {
    std::string type = "none";
    size_t originalBytes = 0;
    size_t processedBytes = 0;
    double preprocessingTimeMs = 0.0;
    double improvement = 0.0;
    bool applied = false;
};

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
    PreprocessMetrics preprocess;

    double ratio() const {
        return compressedBytes > 0
            ? static_cast<double>(originalBytes) / compressedBytes
            : 0.0;
    }

    double totalRatio() const {
        if (!preprocess.applied || preprocess.originalBytes == 0 || compressedBytes == 0) {
            return ratio();
        }
        return static_cast<double>(preprocess.originalBytes) / compressedBytes;
    }
};

} // namespace mosqueeze::bench
