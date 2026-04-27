#pragma once

#include "Terminal.hpp"

#include <fmt/format.h>

#include <chrono>
#include <iostream>
#include <string>

namespace mosqueeze::cli {

class ProgressIndicator {
public:
    explicit ProgressIndicator(const Terminal& term)
        : term_(term) {}

    // Show a "Processing..." message for large files (called before the operation)
    void showProcessing(const std::string& operation, const std::string& filename, size_t fileSize) {
        if (!term_.isTerminal()) {
            return;
        }

        std::string sizeStr;
        if (fileSize >= 1024ULL * 1024 * 1024) {
            sizeStr = fmt::format("{:.1f} GB", static_cast<double>(fileSize) / (1024.0 * 1024.0 * 1024.0));
        } else if (fileSize >= 1024 * 1024) {
            sizeStr = fmt::format("{:.1f} MB", static_cast<double>(fileSize) / (1024.0 * 1024.0));
        } else if (fileSize >= 1024) {
            sizeStr = fmt::format("{:.1f} KB", static_cast<double>(fileSize) / 1024.0);
        } else {
            sizeStr = fmt::format("{} bytes", fileSize);
        }

        start_ = std::chrono::steady_clock::now();
        fmt::print("{}{} {} {} ({})...{}",
                   term_.cyan(), operation, term_.reset(),
                   filename, sizeStr,
                   term_.supportsAnsi() ? "\r\x1b[2K" : "\n");
        std::cout.flush();
        showing_ = true;
    }

    // Clear the processing message after completion
    void clearProcessing() {
        if (!showing_ || !term_.isTerminal()) {
            return;
        }
        if (term_.supportsAnsi()) {
            fmt::print("\x1b[2K\r");
            std::cout.flush();
        }
        showing_ = false;
    }

    // Get elapsed time since showProcessing was called
    double elapsedSeconds() const {
        if (start_ == std::chrono::steady_clock::time_point{}) {
            return 0.0;
        }
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - start_).count();
    }

private:
    const Terminal& term_;
    std::chrono::steady_clock::time_point start_{};
    bool showing_ = false;
};

} // namespace mosqueeze::cli