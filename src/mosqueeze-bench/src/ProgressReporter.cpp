#include <mosqueeze/bench/ProgressReporter.hpp>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace mosqueeze::bench {
namespace {

constexpr size_t kBarWidth = 30;

} // namespace

ProgressReporter::ProgressReporter(size_t totalFiles, bool verbose, bool quiet)
    : totalFiles_(totalFiles), verbose_(verbose), quiet_(quiet) {
    startTime_ = std::chrono::steady_clock::now();
}

ProgressReporter::~ProgressReporter() {
    if (!quiet_ && printedLine_) {
        std::cout << '\n';
    }
}

void ProgressReporter::onProgress(const ProgressInfo& info) {
    if (quiet_) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!info.currentFile.empty()) {
            currentFile_ = info.currentFile.filename().string();
        }
        if (!info.currentAlgorithm.empty()) {
            currentAlgorithm_ = info.currentAlgorithm;
        }
        if (info.currentLevel > 0) {
            currentLevel_ = info.currentLevel;
        }
        currentIteration_ = info.currentIteration;
        totalIterations_ = info.totalIterations;

        if (info.totalFiles > 0) {
            totalFiles_ = static_cast<size_t>(info.totalFiles);
        }
        if (info.completedFiles >= 0) {
            completedFiles_.store(static_cast<size_t>(info.completedFiles), std::memory_order_relaxed);
        }

        if (info.progress > 0.0) {
            progress_ = std::clamp(info.progress, 0.0, 1.0);
        } else if (totalFiles_ > 0) {
            const double completed = static_cast<double>(completedFiles_.load(std::memory_order_relaxed));
            progress_ = std::clamp(completed / static_cast<double>(totalFiles_), 0.0, 1.0);
        }
    }

    updateDisplay(false);
}

void ProgressReporter::reportError(const std::string& error) {
    if (quiet_) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (printedLine_) {
        std::cerr << '\n';
    }
    std::cerr << "Error: " << error << '\n';
}

void ProgressReporter::updateDisplay(bool force) {
    if (quiet_ || totalFiles_ == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    if (!force && lastRender_ != std::chrono::steady_clock::time_point{} &&
        (now - lastRender_) < std::chrono::milliseconds(100)) {
        return;
    }

    std::string line = buildLine();
    if (line.size() < lastLineLength_) {
        line.append(lastLineLength_ - line.size(), ' ');
    }
    lastLineLength_ = line.size();
    lastRender_ = now;
    printedLine_ = true;
    std::cout << '\r' << line << std::flush;
}

std::string ProgressReporter::buildLine() const {
    const size_t completed = completedFiles_.load(std::memory_order_relaxed);
    const double pct = progress_ * 100.0;
    const size_t filled = static_cast<size_t>(progress_ * static_cast<double>(kBarWidth));

    std::ostringstream out;
    out << "Progress [";
    for (size_t i = 0; i < kBarWidth; ++i) {
        out << (i < filled ? '#' : '-');
    }
    out << "] " << completed << "/" << totalFiles_ << " ("
        << std::fixed << std::setprecision(0) << pct << "%)";

    if (!currentFile_.empty()) {
        out << " | " << shortenFile(currentFile_);
    }
    if (!currentAlgorithm_.empty()) {
        out << " | " << currentAlgorithm_;
        if (currentLevel_ > 0) {
            out << " L" << currentLevel_;
        }
    }
    if (verbose_ && totalIterations_ > 0) {
        out << " | iter " << currentIteration_ << "/" << totalIterations_;
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startTime_);
    if (completed > 0 && completed < totalFiles_) {
        const double avgPerFile = static_cast<double>(elapsed.count()) / static_cast<double>(completed);
        const auto remainingFiles = static_cast<double>(totalFiles_ - completed);
        const auto etaSec = std::chrono::seconds(static_cast<long long>(avgPerFile * remainingFiles));
        out << " | ETA " << formatEta(etaSec);
    }

    return out.str();
}

std::string ProgressReporter::formatEta(std::chrono::seconds remaining) const {
    long long total = remaining.count();
    if (total < 0) {
        total = 0;
    }
    if (total < 60) {
        return std::to_string(total) + "s";
    }
    const long long minutes = total / 60;
    const long long seconds = total % 60;
    if (minutes < 60) {
        return std::to_string(minutes) + "m " + std::to_string(seconds) + "s";
    }
    const long long hours = minutes / 60;
    const long long mins = minutes % 60;
    return std::to_string(hours) + "h " + std::to_string(mins) + "m";
}

std::string ProgressReporter::shortenFile(const std::string& file) const {
    constexpr size_t kMax = 40;
    if (file.size() <= kMax) {
        return file;
    }
    return file.substr(0, kMax - 3) + "...";
}

} // namespace mosqueeze::bench
