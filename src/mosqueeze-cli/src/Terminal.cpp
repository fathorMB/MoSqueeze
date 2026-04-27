#include "Terminal.hpp"

#include <fmt/format.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/ioctl.h>
#endif

namespace mosqueeze::cli {

Terminal::Terminal()
    : isTerminal_(detectTerminal())
    , supportsAnsi_(detectAnsiSupport())
    , width_(detectWidth()) {}

bool Terminal::detectAnsiSupport() const {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr) {
        return false;
    }

    DWORD mode = 0;
    if (GetConsoleMode(hOut, &mode) == 0) {
        return false;
    }

    // Try to enable virtual terminal processing (Windows 10+)
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (SetConsoleMode(hOut, mode) == 0) {
        return false;
    }
    return true;
#else
    if (!isTerminal_) {
        return false;
    }
    const char* term = std::getenv("TERM");
    if (!term) {
        return false;
    }
    std::string_view tv(term);
    return tv != "dumb" && tv != "unknown";
#endif
}

bool Terminal::detectTerminal() const {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(STDOUT_FILENO) != 0;
#endif
}

int Terminal::detectWidth() const {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info) != 0) {
        int w = info.srWindow.Right - info.srWindow.Left + 1;
        if (w > 0) {
            return w;
        }
    }
    return 80;
#else
    struct winsize w {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return static_cast<int>(w.ws_col);
    }
    return 80;
#endif
}

std::string Terminal::cyan() const { return supportsAnsi_ ? "\033[36m" : ""; }
std::string Terminal::green() const { return supportsAnsi_ ? "\033[32m" : ""; }
std::string Terminal::red() const { return supportsAnsi_ ? "\033[31m" : ""; }
std::string Terminal::yellow() const { return supportsAnsi_ ? "\033[33m" : ""; }
std::string Terminal::bold() const { return supportsAnsi_ ? "\033[1m" : ""; }
std::string Terminal::reset() const { return supportsAnsi_ ? "\033[0m" : ""; }

void Terminal::printInfo(const std::string& msg) const {
    fmt::print("{}{}{}\n", cyan(), msg, reset());
}

void Terminal::printSuccess(const std::string& msg) const {
    fmt::print("{}✓ {}{}\n", green(), msg, reset());
}

void Terminal::printError(const std::string& msg) const {
    fmt::print(stderr, "{}✗ {}{}\n", red(), msg, reset());
}

void Terminal::printWarning(const std::string& msg) const {
    fmt::print("{}! {}{}\n", yellow(), msg, reset());
}

} // namespace mosqueeze::cli