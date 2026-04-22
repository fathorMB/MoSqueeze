# Worker Spec: Add Progress Feedback During Benchmark Execution

**Issue:** [#45](https://github.com/fathorMB/MoSqueeze/issues/45)
**Branch:** `feat/progress-feedback`
**Priority:** Medium
**Estimated Effort:** ~300 LOC, 8-10 hours

---

## Problem Statement

During benchmark runs, there's no feedback about progress. For long-running benchmarks (especially with ZPAQ on large files), users have no visibility into:
- Which file is currently being processed
- How many files have been completed vs total
- Estimated time remaining

This creates poor UX - users can't estimate completion time or detect stuck processes.

---

## Solution Overview

Add a progress bar component to `mosqueeze-bench` that updates in-place during benchmark execution, showing:
1. Current file X/Y counter (e.g., "15/30 files")
2. Current file name and algorithm/level
3. Optional ETA based on average processing time

---

## Implementation Plan

### 1. Create ProgressReporter Class

**File:** `src/mosqueeze-bench/src/ProgressReporter.hpp`

```cpp
#pragma once

#include <chrono>
#include <string>
#include <atomic>
#include <mutex>

class ProgressReporter {
public:
    ProgressReporter(size_t totalFiles, bool verbose = false, bool quiet = false);
    ~ProgressReporter();

    void startFile(const std::string& filename, const std::string& algorithm, int level);
    void completeFile();
    void reportError(const std::string& error);

private:
    void updateDisplay();
    std::string formatETA(std::chrono::seconds remaining) const;
    std::string formatDuration(std::chrono::seconds duration) const;

    size_t m_totalFiles;
    std::atomic<size_t> m_completedFiles{0};
    std::string m_currentFile;
    std::string m_currentAlgorithm;
    int m_currentLevel{0};

    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_fileStartTime;

    bool m_verbose{false};
    bool m_quiet{false};
    std::mutex m_mutex;

    static constexpr size_t BAR_WIDTH = 30;
};
```

### 2. Create ProgressReporter Implementation

**File:** `src/mosqueeze-bench/src/ProgressReporter.cpp`

```cpp
#include "ProgressReporter.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

ProgressReporter::ProgressReporter(size_t totalFiles, bool verbose, bool quiet)
    : m_totalFiles(totalFiles)
    , m_verbose(verbose)
    , m_quiet(quiet)
{
    m_startTime = std::chrono::steady_clock::now();
}

ProgressReporter::~ProgressReporter() {
    if (!m_quiet && m_totalFiles > 0) {
        std::cout << "\n"; // New line after progress bar
    }
}

void ProgressReporter::startFile(const std::string& filename, const std::string& algorithm, int level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentFile = filename;
    m_currentAlgorithm = algorithm;
    m_currentLevel = level;
    m_fileStartTime = std::chrono::steady_clock::now();
    updateDisplay();
}

void ProgressReporter::completeFile() {
    m_completedFiles++;
    if (m_quiet) return;
    updateDisplay();
}

void ProgressReporter::reportError(const std::string& error) {
    if (m_quiet) return;
    std::cerr << "\nError: " << error << std::endl;
}

void ProgressReporter::updateDisplay() {
    if (m_quiet || m_totalFiles == 0) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime);
    
    // Calculate completion percentage
    size_t completed = m_completedFiles.load();
    double percentage = static_cast<double>(completed) / m_totalFiles * 100.0;

    // Build progress bar
    size_t filled = static_cast<size_t>(percentage / 100.0 * BAR_WIDTH);
    std::stringstream bar;
    bar << "[";
    for (size_t i = 0; i < BAR_WIDTH; ++i) {
        bar << (i < filled ? "█" : "░");
    }
    bar << "]";

    // Estimate ETA
    std::string etaStr;
    if (completed > 0) {
        double avgTimePerFile = static_cast<double>(elapsed.count()) / completed;
        double remainingFiles = m_totalFiles - completed;
        auto etaSeconds = static_cast<long long>(avgTimePerFile * remainingFiles);
        etaStr = formatETA(std::chrono::seconds(etaSeconds));
    } else {
        etaStr = "calculating...";
    }

    // Build output line
    std::stringstream line;
    line << "\rProcessing: " << bar.str() << " " << completed << "/" << m_totalFiles 
         << " (" << std::fixed << std::setprecision(0) << percentage << "%)";

    if (!m_currentFile.empty()) {
        line << " | Current: " << m_currentFile << " (" << m_currentAlgorithm;
        if (m_currentLevel > 0) {
            line << " L" << m_currentLevel;
        }
        line << ")";
    }

    line << " | ETA: " << etaStr;

    // Pad to clear previous line if longer
    static size_t lastLength = 0;
    if (line.str().length() < lastLength) {
        line << std::string(lastLength - line.str().length(), ' ');
    }
    lastLength = line.str().length();

    std::cout << line.str() << std::flush;
}

std::string ProgressReporter::formatETA(std::chrono::seconds remaining) const {
    auto seconds = remaining.count();
    if (seconds < 0) return "N/A";
    if (seconds < 60) return std::to_string(seconds) + "s";

    auto minutes = seconds / 60;
    auto secs = seconds % 60;
    if (minutes < 60) {
        return std::to_string(minutes) + "m " + std::to_string(secs) + "s";
    }

    auto hours = minutes / 60;
    auto mins = minutes % 60;
    return std::to_string(hours) + "h " + std::to_string(mins) + "m";
}

std::string ProgressReporter::formatDuration(std::chrono::seconds duration) const {
    return formatETA(duration);
}
```

### 3. Integrate into BenchmarkRunner

**File:** `src/mosqueeze-bench/src/BenchmarkRunner.cpp`

Add member variable:
```cpp
#include "ProgressReporter.hpp"

// In class BenchmarkRunner:
private:
    std::unique_ptr<ProgressReporter> m_progress;
```

Initialize in `run()`:
```cpp
BenchmarkResult BenchmarkRunner::run(const BenchmarkConfig& config) {
    m_progress = std::make_unique<ProgressReporter>(
        config.files.size(), 
        config.verbose, 
        config.quiet
    );
    
    // ... existing code ...
    
    for (const auto& file : config.files) {
        m_progress->startFile(file.filename(), engine->name(), level);
        
        // ... compression code ...
        
        m_progress->completeFile();
    }
    
    return result;
}
```

### 4. Update CLI Options

**File:** `src/mosqueeze-bench/src/main.cpp`

Progress is automatic (no new flags needed). It respects existing flags:
- `--verbose`: Shows more detailed output (progress still shown)
- `--quiet`: Disables progress output entirely

### 5. Add Unit Tests

**File:** `tests/unit/ProgressReporter_test.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include "ProgressReporter.hpp"

TEST_CASE("ProgressReporter initialization", "[progress]") {
    ProgressReporter reporter(10, false, false);
    // Test: initializes with correct total
}

TEST_CASE("ProgressReporter quiet mode", "[progress]") {
    ProgressReporter reporter(10, false, true);
    reporter.startFile("test.raf", "zstd", 3);
    reporter.completeFile();
    // Test: no output in quiet mode
}

TEST_CASE("ProgressReporter percentage calculation", "[progress]") {
    ProgressReporter reporter(100, false, false);
    for (int i = 0; i < 50; ++i) {
        reporter.completeFile();
    }
    // Test: should show 50%
}

TEST_CASE("ProgressReporter ETA estimation", "[progress]") {
    ProgressReporter reporter(10, false, false);
    // Test: ETA appears after first file completes
}
```

---

## Acceptance Criteria

- [ ] Progress display shows current file / total files during benchmark
- [ ] Progress updates in-place (uses `\r` carriage return, no spam new lines)
- [ ] Works with multi-threaded execution (thread-safe updates)
- [ ] Respects `--quiet` flag (no progress output when quiet)
- [ ] ETA appears after at least 1 file completes
- [ ] Progress bar fills visually as completion percentage increases
- [ ] Current file name + algorithm displayed during processing
- [ ] Clean newline output after benchmark completes

---

## Testing Plan

1. **Unit tests** for ProgressReporter class
2. **Manual test** with small dataset (5 files) to verify progress bar
3. **Manual test** with `--quiet` to ensure no output
4. **Manual test** with multi-threaded execution (`--threads 4`)
5. **Visual test** in different terminal sizes (narrow/wide)
6. **Integration test** with long-running benchmark (ZPAQ on large files)

---

## Edge Cases

- Empty file list: Show warning, no progress
- Single file: Skip ETA (not enough data)
- Multi-threaded: Use mutex for thread-safe counter updates
- Large file counts (>1000): Consider updating display every N files to avoid overhead

---

## Dependencies

- Standard library only (`<iostream>`, `<chrono>`, `<mutex>`)
- No external dependencies

---

## Files to Create/Modify

| File | Action |
|------|--------|
| `src/mosqueeze-bench/src/ProgressReporter.hpp` | Create |
| `src/mosqueeze-bench/src/ProgressReporter.cpp` | Create |
| `src/mosqueeze-bench/src/BenchmarkRunner.cpp` | Modify |
| `src/mosqueeze-bench/src/main.cpp` | Modify (if needed) |
| `tests/unit/ProgressReporter_test.cpp` | Create |
| `tests/CMakeLists.txt` | Modify (add test) |

---

## Notes

- Progress bar uses Unicode block characters (█ ░) which work in most modern terminals
- Fallback to ASCII (`[=  ]`) can be added for compatibility
- This is a quality-of-life improvement, not a functional change
- Low risk of breaking existing behavior