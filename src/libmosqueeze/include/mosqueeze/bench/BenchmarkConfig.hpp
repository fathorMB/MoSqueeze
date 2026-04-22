#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace mosqueeze::bench {

struct ProgressInfo {
    std::filesystem::path currentFile;
    std::string currentAlgorithm;
    int currentLevel = 0;
    int currentIteration = 0;
    int totalIterations = 0;
    int totalFiles = 0;
    int completedFiles = 0;
    double progress = 0.0;
};

struct BenchmarkStats {
    double meanRatio = 0.0;
    double stdDevRatio = 0.0;
    double meanEncodeTime = 0.0;
    double stdDevEncodeTime = 0.0;
    double meanDecodeTime = 0.0;
    double stdDevDecodeTime = 0.0;
    double meanPeakMemory = 0.0;
    int iterations = 0;
};

struct BenchmarkConfig {
    std::vector<std::filesystem::path> files;
    bool useStdin = false;
    std::filesystem::path corpusPath{"benchmarks/corpus"};

    std::vector<std::string> algorithms;
    std::vector<int> levels;
    bool allEngines = false;
    bool defaultOnly = false;

    int iterations = 1;
    int warmupIterations = 0;
    // Track peak memory usage during compression.
    // Note: parallel runs disable this automatically because memory sampling
    // is process-wide and cannot be attributed to individual file operations.
    bool trackMemory = true;
    bool runDecode = true;
    std::chrono::seconds maxTimePerFile{3600};

    int threadCount = 0;
    bool sequential = false;

    std::function<void(const ProgressInfo&)> onProgress;
    std::function<bool()> shouldCancel;

    bool hasProgressCallback() const { return static_cast<bool>(onProgress); }
    bool hasCancellation() const { return static_cast<bool>(shouldCancel); }
    int getEffectiveThreadCount() const {
        if (sequential) {
            return 1;
        }
        if (threadCount > 0) {
            return threadCount;
        }
        const int hw = static_cast<int>(std::thread::hardware_concurrency());
        return hw > 0 ? hw : 1;
    }
};

} // namespace mosqueeze::bench
