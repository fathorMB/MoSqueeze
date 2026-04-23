#include <mosqueeze/bench/ProgressReporter.hpp>

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace mosqueeze::bench {
namespace {

constexpr size_t kBarWidth = 30;

} // namespace

ProgressReporter::ProgressReporter(size_t totalFiles, bool verbose, bool quiet)
    : totalFiles_(totalFiles), verbose_(verbose), quiet_(quiet) {
    startTime_ = std::chrono::steady_clock::now();
    lastActivity_ = startTime_;
    ansiEnabled_ = detectAnsiSupport();
    terminalWidth_ = detectTerminalWidth();
    if (!quiet_ && totalFiles_ > 0) {
        renderThread_ = std::thread([this]() {
            std::unique_lock<std::mutex> lock(mutex_);
            while (!stopRequested_.load(std::memory_order_relaxed)) {
                wakeCv_.wait_for(lock, refreshInterval_);
                if (stopRequested_.load(std::memory_order_relaxed)) {
                    break;
                }
                lock.unlock();
                updateDisplay(true);
                lock.lock();
            }
        });
    }
}

ProgressReporter::~ProgressReporter() {
    stopRequested_.store(true, std::memory_order_relaxed);
    wakeCv_.notify_all();
    if (renderThread_.joinable()) {
        renderThread_.join();
    }
    updateDisplay(true);
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
        lastActivity_ = std::chrono::steady_clock::now();
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
    wakeCv_.notify_all();
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
    std::lock_guard<std::mutex> lock(mutex_);
    if (quiet_ || totalFiles_ == 0) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (!force && lastRender_ != std::chrono::steady_clock::time_point{} &&
        (now - lastRender_) < std::chrono::milliseconds(100)) {
        return;
    }

    std::string line = fitToTerminalWidth(buildLine());
    if (!ansiEnabled_ && line.size() < lastLineLength_) {
        line.append(lastLineLength_ - line.size(), ' ');
    }
    lastLineLength_ = line.size();
    lastRender_ = now;
    printedLine_ = true;
    if (ansiEnabled_) {
        std::cout << "\r\x1b[2K" << line << std::flush;
    } else {
        std::cout << '\r' << line << std::flush;
    }
}

std::string ProgressReporter::buildLine() const {
    const size_t completed = completedFiles_.load(std::memory_order_relaxed);
    const double pct = progress_ * 100.0;
    const size_t filled = static_cast<size_t>(progress_ * static_cast<double>(kBarWidth));
    const char spinner = kSpinner[spinnerIndex_ % 4];

    std::ostringstream out;
    out << spinner << " Progress [";
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
    out << " | elapsed " << formatEta(elapsed);
    if (completed > 0 && completed < totalFiles_) {
        const double avgPerFile = static_cast<double>(elapsed.count()) / static_cast<double>(completed);
        const auto remainingFiles = static_cast<double>(totalFiles_ - completed);
        const auto etaSec = std::chrono::seconds(static_cast<long long>(avgPerFile * remainingFiles));
        out << " | ETA " << formatEta(etaSec);
    } else if (completed >= totalFiles_ && totalFiles_ > 0) {
        out << " | ETA done";
    }

    ++spinnerIndex_;
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

std::string ProgressReporter::fitToTerminalWidth(std::string line) const {
    // Reserve one column to avoid implicit terminal wrapping that can create
    // duplicated progress rows when repeatedly writing '\r' updates.
    if (terminalWidth_ == 0 || terminalWidth_ <= 8) {
        return line;
    }
    const size_t maxVisible = terminalWidth_ - 1;
    if (line.size() <= maxVisible) {
        return line;
    }
    if (maxVisible <= 3) {
        return line.substr(0, maxVisible);
    }
    return line.substr(0, maxVisible - 3) + "...";
}

size_t ProgressReporter::detectTerminalWidth() const {
    const char* columns = std::getenv("COLUMNS");
    if (columns != nullptr) {
        try {
            const size_t parsed = static_cast<size_t>(std::stoul(columns));
            if (parsed >= 20) {
                return parsed;
            }
        } catch (...) {
        }
    }

#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi) != 0) {
        const SHORT width = static_cast<SHORT>(csbi.srWindow.Right - csbi.srWindow.Left + 1);
        if (width > 0) {
            return static_cast<size_t>(width);
        }
    }
#else
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return static_cast<size_t>(ws.ws_col);
    }
#endif

    return 120;
}

bool ProgressReporter::detectAnsiSupport() const {
#ifdef _WIN32
    HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdoutHandle == INVALID_HANDLE_VALUE || stdoutHandle == nullptr) {
        return false;
    }

    DWORD mode = 0;
    if (GetConsoleMode(stdoutHandle, &mode) == 0) {
        return false;
    }

    const DWORD wanted = mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (SetConsoleMode(stdoutHandle, wanted) == 0) {
        return false;
    }
    return true;
#else
    return true;
#endif
}

} // namespace mosqueeze::bench
