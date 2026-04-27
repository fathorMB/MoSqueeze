# Worker Spec: Cross-Platform CLI Application (#12)

## Issue
https://github.com/fathorMB/MoSqueeze/issues/12

## Overview
Complete the cross-platform CLI application with all required commands, proper exit codes, progress indication, and cross-platform compatibility.

## Current State

### ✅ Already Implemented (via separate issues)
| Command | Status | Issue |
|---------|--------|-------|
| `mosqueeze compress` | ✅ Complete | #67 |
| `mosqueeze decompress` | ✅ Complete | #67 |
| `mosqueeze analyze` | ✅ Complete | main.cpp |
| `mosqueeze suggest` | ✅ Complete | main.cpp (IntelligentSelector) |
| `mosqueeze predict` | ✅ Complete | #101 |

### ❌ Missing Components
| Component | Status | Notes |
|-----------|--------|-------|
| `mosqueeze benchmark` | ❌ Missing | mosqueeze-bench is a separate tool |
| Cross-platform verification | ❌ Missing | Not tested on all platforms |
| Exit codes | ❌ Incomplete | Need standardized exit codes |
| Progress indication | ❌ Missing | No progress for compress/decompress |
| ANSI color graceful degradation | ❌ Missing | No fallback for non-ANSI terminals |

## Requirements

### 1. Add `benchmark` Subcommand

Currently, `mosqueeze-bench` is a SEPARATE tool. User experience is fragmented:
- `mosqueeze compress` → one tool
- `mosqueeze analyze` → one tool
- `mosqueeze-bench -d ./corpus` → ANOTHER tool

**Solution:** Add `benchmark` subcommand that wraps `mosqueeze-bench` functionality OR document clearly that benchmark is a separate tool.

**Option A (Recommended - Wrapper):**
```cpp
// Reuse BenchmarkRunner from mosqueeze-bench
void addBenchmarkCommand(CLI::App& app) {
    auto* bench = app.add_subcommand("benchmark", "Run compression benchmarks");
    
    // Same flags as mosqueeze-bench
    bench->add_option("-d,--directory", ...);
    bench->add_option("-a,--algorithms", ...);
    bench->add_flag("--default-only", ...);
    // ...
    
    bench->callback([]() {
        // Delegate to BenchmarkRunner
    });
}
```

**Option B (Documentation):**
Add to CLI help:
```
Note: For benchmarking compression algorithms across multiple files,
use the separate `mosqueeze-bench` tool:

  mosqueeze-bench -d ./corpus -a zstd,xz,brotli,zpaq --default-only
```

### 2. Exit Codes

Standardize exit codes across all commands:

| Code | Meaning | Usage |
|------|---------|-------|
| 0 | Success | Operation completed successfully |
| 1 | Error | Input/output errors, compression failures |
| 2 | Cancelled | User interrupted (Ctrl+C, SIGTERM) |
| 3 | Invalid arguments | CLI11 validation failures |

**Implementation:**

```cpp
// src/mosqueeze-cli/src/main.cpp

#include <csignal>
#include <atomic>

namespace {
std::atomic<bool> g_interrupted{false};

void signalHandler(int) {
    g_interrupted = true;
}
}

// In each command callback:
int main(int argc, char** argv) {
    std::signal(SIGINT, [](int) { g_interrupted = true; });
    std::signal(SIGTERM, [](int) { g_interrupted = true; });
    
    CLI::App app{"MoSqueeze - Cold Storage Compression Library"};
    
    // ... setup commands ...
    
    try {
        CLI11_PARSE(app, argc, argv);
        
        if (g_interrupted) {
            spdlog::error("Operation cancelled by user");
            return 2; // Cancelled
        }
        return 0; // Success
        
    } catch (const CLI::ParseError& e) {
        // CLI11 handles this internally, returns non-zero
        return app.exit(e);
    } catch (const std::exception& e) {
        spdlog::error("Error: {}", e.what());
        return 1; // Error
    }
}
```

### 3. Progress Indication for Long Operations

**CompressCommand / DecompressCommand need progress bars:**

```cpp
// For files > 1MB, show progress

class ProgressCallback {
public:
    virtual void onProgress(double progress, size_t bytesProcessed, size_t totalBytes) = 0;
};

// In CompressionPipeline:
void setProgressCallback(ProgressCallback* callback);

// ConsoleProgressCallback implementation
class ConsoleProgressCallback : public ProgressCallback {
public:
    void onProgress(double progress, size_t bytesProcessed, size_t totalBytes) override {
        if (!terminalSupportsAnsi_) {
            return; // Graceful degradation
        }
        
        const int barWidth = 40;
        const int filled = static_cast<int>(progress * barWidth);
        
        fmt::print("\r  [");
        for (int i = 0; i < barWidth; ++i) {
            fmt::print(i < filled ? "=" : " ");
        }
        fmt::print("] {:.0f}% ({:.1f} MB / {:.1f} MB)", 
            progress * 100,
            bytesProcessed / (1024.0 * 1024.0),
            totalBytes / (1024.0 * 1024.0));
        
        if (progress >= 1.0) {
            fmt::print("\n");
        }
    }
    
private:
    bool terminalSupportsAnsi_ = detectAnsiSupport();
};
```

### 4. ANSI Color Support with Graceful Degradation

**Detection logic:**

```cpp
// src/mosqueeze-cli/src/Terminal.hpp

#pragma once

#include <string>

namespace mosqueeze::cli {

class Terminal {
public:
    Terminal();
    
    bool supportsAnsi() const { return supportsAnsi_; }
    bool isTerminal() const { return isTerminal_; }
    int width() const { return width_; }
    
    // Color codes (empty if ANSI not supported)
    std::string cyan() const;
    std::string green() const;
    std::string red() const;
    std::string yellow() const;
    std::string reset() const;
    
    // Safe print functions
    void printInfo(const std::string& msg);
    void printSuccess(const std::string& msg);
    void printError(const std::string& msg);
    void printWarning(const std::string& msg);
    
private:
    bool detectAnsiSupport() const;
    bool detectTerminal() const;
    int detectWidth() const;
    
    bool supportsAnsi_;
    bool isTerminal_;
    int width_;
};

} // namespace mosqueeze::cli
```

**Implementation:**

```cpp
// src/mosqueeze-cli/src/Terminal.cpp

#include "Terminal.hpp"
#include <fmt/format.h>

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
#else
    #include <unistd.h>
    #include <sys/ioctl.h>
#endif

namespace mosqueeze::cli {

Terminal::Terminal() 
    : supportsAnsi_(detectAnsiSupport())
    , isTerminal_(detectTerminal())
    , width_(detectWidth()) 
{}

bool Terminal::detectAnsiSupport() const {
#ifdef _WIN32
    // Windows 10+ supports ANSI with virtual terminal sequences
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return false;
    
    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) return false;
    
    // Try to enable ANSI
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, mode)) return false;
    
    return true;
#else
    // Unix terminals: check TERM and isatty
    if (!isTerminal_) return false;
    
    const char* term = std::getenv("TERM");
    if (!term) return false;
    
    // Common terminals that support ANSI
    std::string_view termView(term);
    if (termView == "dumb") return false;
    if (termView == "unknown") return false;
    
    return true;
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
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(hOut, &info)) {
        return info.srWindow.Right - info.srWindow.Left + 1;
    }
    return 80;
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_col;
    }
    return 80;
#endif
}

std::string Terminal::cyan() const { return supportsAnsi_ ? "\033[36m" : ""; }
std::string Terminal::green() const { return supportsAnsi_ ? "\033[32m" : ""; }
std::string Terminal::red() const { return supportsAnsi_ ? "\033[31m" : ""; }
std::string Terminal::yellow() const { return supportsAnsi_ ? "\033[33m" : ""; }
std::string Terminal::reset() const { return supportsAnsi_ ? "\033[0m" : ""; }

void Terminal::printInfo(const std::string& msg) {
    fmt::print("{}{}{}\n", cyan(), msg, reset());
}

void Terminal::printSuccess(const std::string& msg) {
    fmt::print("{}✓ {}{}\n", green(), msg, reset());
}

void Terminal::printError(const std::string& msg) {
    fmt::print(stderr, "{}✗ {}{}\n", red(), msg, reset());
}

void Terminal::printWarning(const std::string& msg) {
    fmt::print("{}! {}{}\n", yellow(), msg, reset());
}

} // namespace mosqueeze::cli
```

### 5. Command Documentation

Update `--help` output with examples:

```cpp
auto* compress = app.add_subcommand("compress", "Compress a file");

compress->footer(R"(
Examples:
  mosqueeze compress input.dat -o output.msz -a zstd
  mosqueeze compress large.json -o compact.msz -a brotli -l 11
  mosqueeze compress photo.raf -o photo.msz --preprocess bayer-raw
  
Algorithms:
  zstd    - Fast, good compression (levels 1-22, default: 3)
  brotli  - Good for text (levels 0-11, default: 6)
  lzma    - High compression (levels 0-9, default: 6)
  zpaq    - Best ratio, slow (levels 1-5, default: 3)
)");
```

## Files to Create

### 1. `src/mosqueeze-cli/src/Terminal.hpp` (NEW)
Terminal detection and color support.

### 2. `src/mosqueeze-cli/src/Terminal.cpp` (NEW)
Cross-platform terminal support implementation.

### 3. `src/mosqueeze-cli/src/ProgressCallback.hpp` (NEW)
Progress callback interface for long operations.

### 4. `src/mosqueeze-cli/src/commands/BenchmarkCommand.cpp` (NEW - Optional)
If implementing benchmark as subcommand.

## Files to Modify

### 1. `src/mosqueeze-cli/src/main.cpp`
- Add signal handlers for Ctrl+C
- Standardize exit codes
- Update help text with examples

### 2. `src/mosqueeze-cli/src/commands/CompressCommand.cpp`
- Add progress indication for files > 1MB
- Use Terminal class for colored output

### 3. `src/mosqueeze-cli/src/commands/DecompressCommand.cpp`
- Add progress indication
- Use Terminal class for colored output

### 4. `src/mosqueeze-cli/CMakeLists.txt`
- Add new source files

## Acceptance Criteria

- [ ] All existing commands work correctly (compress, decompress, analyze, predict, suggest)
- [ ] Exit codes: 0=success, 1=error, 2=cancelled (Ctrl+C), 3=invalid args
- [ ] Progress indication shows for files > 1MB during compress/decompress
- [ ] ANSI colors work on Windows 10+, Linux, macOS terminals
- [ ] Graceful degradation on non-ANSI terminals (dumb, pipe, redirect)
- [ ] `--help` includes examples for each command
- [ ] Works on Windows, Linux, macOS (CI verification)

## Testing Strategy

### Unit Tests
```cpp
// tests/unit/Terminal_test.cpp
TEST(Terminal, SupportsAnsiOnTerminal);
TEST(Terminal, NoAnsiOnPipe);
TEST(Terminal, DetectsWidth);

// tests/unit/ExitCodes_test.cpp
TEST(ExitCodes, SuccessReturnsZero);
TEST(ExitCodes, FileNotFoundReturnsOne);
TEST(ExitCodes, CtrlCReturnsTwo);
TEST(ExitCodes, InvalidArgsReturnThree);
```

### Integration Tests
```bash
# Test exit codes
./mosqueeze compress nonexistent.dat -o out.msz
echo $?  # Should be 1

./mosqueeze compress --invalid-flag
echo $?  # Should be 3

# Test progress on large file (>1MB)
./mosqueeze compress large.dat -o out.msz -a zstd
# Should show progress bar

# Test color output
./mosqueeze compress test.dat -o out.msz | cat
# Should NOT show ANSI codes (piped)

./mosqueeze compress test.dat -o out.msz
# Should show ANSI colors (terminal)

# Test Ctrl+C handling
./mosqueeze compress huge.dat -o out.msz &
PID=$!
sleep 1
kill -INT $PID
# Should exit with code 2
```

## Estimated Effort

| Task | Hours |
|------|-------|
| Terminal class (cross-platform) | 2-3h |
| Exit codes standardization | 1h |
| Progress indication | 2h |
| Benchmark subcommand (optional) | 3-4h |
| Documentation & examples | 1h |
| Testing | 2h |
| **Total** | **11-13h** |

## Dependencies

- All compression engines (closed: #7)
- File type detection (closed: #8)
- Algorithm selection (closed: #11)
- Intelligent Selector (closed: #55)
- Compress/Decompress commands (closed: #67)
- Predict command (closed: #101)

## Notes

### Benchmark Subcommand Decision

**Recommendation:** Do NOT add benchmark as subcommand in this issue.

Reasons:
1. `mosqueeze-bench` is a mature, separate tool with complex options
2. Benchmark has different use case (research, not daily use)
3. Keeping separate reduces CLI complexity

Instead:
- Add a note in `mosqueeze --help` pointing to `mosqueeze-bench`
- Consider this issue complete without benchmark subcommand

### Future Enhancements (Out of Scope)

- `mosqueeze compress --recursive ./folder` - batch compression
- `mosqueeze inspect archive.msz` - show compression metadata
- `mosqueeze compare file1.msz file2.msz` - compare algorithms
- Shell completion (bash, zsh, fish, powershell)