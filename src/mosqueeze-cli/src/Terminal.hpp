#pragma once

#include <string>

namespace mosqueeze::cli {

class Terminal {
public:
    Terminal();

    bool supportsAnsi() const { return supportsAnsi_; }
    bool isTerminal() const { return isTerminal_; }
    int width() const { return width_; }

    // Color codes (empty strings if ANSI not supported)
    std::string cyan() const;
    std::string green() const;
    std::string red() const;
    std::string yellow() const;
    std::string bold() const;
    std::string reset() const;

    // Safe print functions that degrade gracefully on non-ANSI terminals
    void printInfo(const std::string& msg) const;
    void printSuccess(const std::string& msg) const;
    void printError(const std::string& msg) const;
    void printWarning(const std::string& msg) const;

private:
    bool detectAnsiSupport() const;
    bool detectTerminal() const;
    int detectWidth() const;

    bool supportsAnsi_;
    bool isTerminal_;
    int width_;
};

} // namespace mosqueeze::cli