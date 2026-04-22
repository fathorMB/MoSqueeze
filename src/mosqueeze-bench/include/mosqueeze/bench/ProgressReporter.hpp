#pragma once

#include <mosqueeze/bench/BenchmarkConfig.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>

namespace mosqueeze::bench {

class ProgressReporter {
public:
    ProgressReporter(size_t totalFiles, bool verbose = false, bool quiet = false);
    ~ProgressReporter();

    void onProgress(const ProgressInfo& info);
    void reportError(const std::string& error);

private:
    void updateDisplay(bool force);
    std::string buildLine() const;
    std::string formatEta(std::chrono::seconds remaining) const;
    std::string shortenFile(const std::string& file) const;

    size_t totalFiles_ = 0;
    std::atomic<size_t> completedFiles_{0};
    std::string currentFile_;
    std::string currentAlgorithm_;
    int currentLevel_ = 0;
    int currentIteration_ = 0;
    int totalIterations_ = 0;
    double progress_ = 0.0;

    bool verbose_ = false;
    bool quiet_ = false;
    bool printedLine_ = false;
    size_t lastLineLength_ = 0;

    std::chrono::steady_clock::time_point startTime_{};
    std::chrono::steady_clock::time_point lastRender_{};
    mutable std::mutex mutex_;
};

} // namespace mosqueeze::bench
