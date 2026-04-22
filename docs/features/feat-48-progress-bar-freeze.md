# Worker Spec: Fix Progress Bar Freezing During Benchmark Execution

**Issue:** [#48](https://github.com/fathorMB/MoSqueeze/issues/48)
**Branch:** `fix/progress-bar-freeze`
**Priority:** High
**Type:** Bug Fix + Enhancement
**Estimated Effort:** ~350 LOC, 10-14 hours

---

## Bug Description

The progress bar implemented in #45 freezes at 0% during benchmark execution, even though the process is actively running (visible via Task Manager with CPU/disk activity).

### Reproduction

```bash
mosqueeze-bench -d "path/to/files" -a zstd,xz,brotli,zpaq -o results
```

Observed:
```
Processing: [░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░] 0/30 files (0%)
```
Bar stays at 0% for entire duration despite active processing.

---

## Root Causes

### 1. Multi-threaded Execution Issue

The benchmark uses `--threads N` (default: auto-detect). `ProgressReporter` is not thread-safe:

```cpp
// Current implementation - NOT thread-safe!
void completeFile() {
    m_completedFiles++;  // Data race!
}
```

Multiple threads call `completeFile()` concurrently, causing:
- Counter corruption (data races)
- Lost updates (non-atomic reads/writes)
- Display corruption (concurrent console writes)

### 2. Console Output Issue on Windows

Carriage return `\r` may not work correctly in CMD/PowerShell:
- Output buffering prevents immediate display
- No line flushing after each update
- May need `SetConsoleMode` for virtual terminal sequences

### 3. Per-File Granularity

Large files (27MB RAF) with slow algorithms (ZPAQ) take 2-5 minutes each:
- Progress only updates **after** file completion
- No feedback during file processing
- User thinks process is frozen

---

## Solution Overview

### Tier 1: Critical Fixes

1. **Thread-safe progress tracking** (atomic counters, proper synchronization)
2. **Flush stdout after each update** (force immediate display)
3. **Test on Windows CMD/PowerShell**
4. **Mutex-protected console output**

### Tier 2: Improved Granularity

1. **Per-file progress callback** during compression
2. **Update display at regular intervals** (every 1-2 seconds)
3. **Show bytes processed during long operations**

### Tier 3: Enhanced UX (Lower Priority)

1. **ETA estimation** based on average time per file
2. **Throughput display** (MB/s)
3. **Current algorithm name and level**
4. **Color-coded progress** (green = good, yellow = slow)

---

## Implementation Plan

### 1. Fix ProgressReporter for Thread Safety

**File:** `src/mosqueeze-bench/src/ProgressReporter.hpp`

```cpp
#pragma once

#include <chrono>
#include <string>
#include <atomic>
#include <mutex>
#include <iostream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

class ProgressReporter {
public:
    ProgressReporter(size_t totalFiles, size_t totalBytes = 0, bool verbose = false, bool quiet = false);
    ~ProgressReporter();

    void startFile(const std::string& filename, const std::string& algorithm, int level);
    void updateFileProgress(size_t bytesProcessed, size_t totalBytes);
    void completeFile();
    void reportError(const std::string& error);

private:
    void updateDisplay();
    void initConsole();
    std::string formatETA(std::chrono::seconds remaining) const;
    std::string formatSpeed(double bytesPerSecond) const;

    size_t m_totalFiles;
    size_t m_totalBytes;
    std::atomic<size_t> m_completedFiles{0};
    std::atomic<size_t> m_completedBytes{0};
    std::string m_currentFile;
    std::string m_currentAlgorithm;
    int m_currentLevel{0};
    size_t m_currentFileBytes{0};
    std::atomic<size_t> m_currentFileProgress{0};

    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_fileStartTime;
    std::chrono::steady_clock::time_point m_lastUpdate;

    bool m_verbose{false};
    bool m_quiet{false};
    std::mutex m_mutex;

    static constexpr size_t BAR_WIDTH = 30;
    static constexpr auto UPDATE_INTERVAL = std::chrono::milliseconds(500);

#ifdef _WIN32
    HANDLE m_consoleHandle{INVALID_HANDLE_VALUE};
    bool m_vtEnabled{false};
#endif
};
```

### 2. Implement Thread-Safe ProgressReporter

**File:** `src/mosqueeze-bench/src/ProgressReporter.cpp`

```cpp
#include "ProgressReporter.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

ProgressReporter::ProgressReporter(size_t totalFiles, size_t totalBytes, bool verbose, bool quiet)
    : m_totalFiles(totalFiles)
    , m_totalBytes(totalBytes)
    , m_verbose(verbose)
    , m_quiet(quiet)
{
    m_startTime = std::chrono::steady_clock::now();
    m_lastUpdate = m_startTime;
    initConsole();
}

ProgressReporter::~ProgressReporter() {
    if (!m_quiet && m_totalFiles > 0) {
        std::cout << "\n"; // New line after progress bar
        std::cout.flush();
    }
}

void ProgressReporter::initConsole() {
#ifdef _WIN32
    // Enable virtual terminal sequences on Windows for ANSI colors
    m_consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (m_consoleHandle != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(m_consoleHandle, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(m_consoleHandle, mode);
            m_vtEnabled = true;
        }
    }
#endif
}

void ProgressReporter::startFile(const std::string& filename, const std::string& algorithm, int level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentFile = filename;
    m_currentAlgorithm = algorithm;
    m_currentLevel = level;
    m_currentFileBytes = 0;
    m_currentFileProgress = 0;
    m_fileStartTime = std::chrono::steady_clock::now();
    
    // Force immediate update when starting a new file
    updateDisplay();
}

void ProgressReporter::updateFileProgress(size_t bytesProcessed, size_t totalBytes) {
    m_currentFileBytes = totalBytes;
    m_currentFileProgress = bytesProcessed;
    
    // Throttle updates to avoid console spam
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastUpdate);
    
    if (elapsed >= UPDATE_INTERVAL) {
        std::lock_guard<std::mutex> lock(m_mutex);
        updateDisplay();
        m_lastUpdate = now;
    }
}

void ProgressReporter::completeFile() {
    m_completedFiles.fetch_add(1);
    if (m_quiet) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    updateDisplay();
}

void ProgressReporter::reportError(const std::string& error) {
    if (m_quiet) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cerr << "\nError: " << error << std::endl;
}

void ProgressReporter::updateDisplay() {
    if (m_quiet || m_totalFiles == 0) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime);
    
    // Calculate completion percentage
    size_t completed = m_completedFiles.load();
    double percentage = static_cast<double>(completed) / m_totalFiles * 100.0;

    // Calculate speed and ETA
    double bytesPerSecond = 0;
    if (elapsed.count() > 0 && m_completedBytes.load() > 0) {
        bytesPerSecond = static_cast<double>(m_completedBytes.load()) / elapsed.count();
    }

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
    if (completed > 0 && completed < m_totalFiles) {
        double avgTimePerFile = static_cast<double>(elapsed.count()) / completed;
        double remainingFiles = m_totalFiles - completed;
        auto etaSeconds = static_cast<long long>(avgTimePerFile * remainingFiles);
        etaStr = formatETA(std::chrono::seconds(etaSeconds));
    } else if (completed >= m_totalFiles) {
        etaStr = "done";
    } else {
        etaStr = "calculating...";
    }

    // Build speed string
    std::string speedStr;
    if (bytesPerSecond > 0) {
        speedStr = formatSpeed(bytesPerSecond);
    }

    // Build output line
    std::stringstream line;
    line << "\rProcessing: " << bar.str() << " " << completed << "/" << m_totalFiles 
         << " (" << std::fixed << std::setprecision(0) << percentage << "%)";

    if (!m_currentFile.empty()) {
        line << " | " << m_currentFile << " (" << m_currentAlgorithm;
        if (m_currentLevel > 0) {
            line << " L" << m_currentLevel;
        }
        line << ")";
    }

    if (!etaStr.empty()) {
        line << " | ETA: " << etaStr;
    }

    if (!speedStr.empty()) {
        line << " | " << speedStr;
    }

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

std::string ProgressReporter::formatSpeed(double bytesPerSecond) const {
    if (bytesPerSecond < 1024) {
        return std::to_string(static_cast<int>(bytesPerSecond)) + " B/s";
    } else if (bytesPerSecond < 1024 * 1024) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << (bytesPerSecond / 1024) << " KB/s";
        return ss.str();
    } else {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << (bytesPerSecond / 1024 / 1024) << " MB/s";
        return ss.str();
    }
}
```

### 3. Integrate Progress in BenchmarkRunner

**File:** `src/mosqueeze-bench/src/BenchmarkRunner.cpp`

Add progress callback to compression:

```cpp
void BenchmarkRunner::runCompression(/* ... */) {
    // ... existing code ...
    
    if (m_progress) {
        m_progress->startFile(file.filename(), engine->name(), level);
    }
    
    // Compression with progress callback
    size_t bytesProcessed = 0;
    constexpr size_t CHUNK_SIZE = 64 * 1024; // 64KB chunks
    
    while (/* reading input */) {
        // Process chunk
        // ...
        bytesProcessed += chunk.size();
        
        if (m_progress && m_config.verbose) {
            m_progress->updateFileProgress(bytesProcessed, fileSize);
        }
    }
    
    if (m_progress) {
        m_progress->completeFile();
    }
}
```

### 4. Add Progress Callback Interface

**File:** `src/libmosqueeze/include/mosqueeze/CompressionEngine.hpp`

```cpp
class CompressionEngine {
public:
    // ... existing methods ...
    
    // Progress callback type
    using ProgressCallback = std::function<void(size_t bytesProcessed, size_t totalBytes)>;
    
    // Set progress callback for long operations
    virtual void setProgressCallback(ProgressCallback callback) {
        m_progressCallback = std::move(callback);
    }
    
protected:
    ProgressCallback m_progressCallback;
};
```

### 5. Update Unit Tests

**File:** `tests/unit/ProgressReporter_test.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include "ProgressReporter.hpp"
#include <thread>
#include <vector>

TEST_CASE("ProgressReporter thread safety", "[progress]") {
    ProgressReporter reporter(100, 0, false, true); // quiet mode
    
    // Spawn multiple threads completing files
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&reporter, i]() {
            for (int j = 0; j < 10; ++j) {
                reporter.completeFile();
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Counter should be exactly 100
    // No data races should occur
}

TEST_CASE("ProgressReporter ETA calculation", "[progress]") {
    ProgressReporter reporter(10, 0, false, true);
    
    // Complete 5 files
    for (int i = 0; i < 5; ++i) {
        reporter.completeFile();
    }
    
    // ETA should be calculable after completing files
    // (implementation specific)
}

TEST_CASE("ProgressReporter displays during long operation", "[progress]") {
    // Test that progress updates are shown during file processing
    // Not just at completion
}
```

---

## Testing Plan

### Manual Testing

```bash
# Test with multiple threads
mosqueeze-bench -d "test/files" -a zstd --threads 4 -v

# Test with single file (progress during one file)
mosqueeze-bench -f "large.raf" -a zpaq -v

# Test with quiet mode
mosqueeze-bench -d "test/files" -a zstd --quiet

# Test on Windows CMD
mosqueeze-bench -d "test/files" -a zstd

# Test on Windows PowerShell
powershell> .\mosqueeze-bench.exe -d "test/files" -a zstd
```

### Automated Testing

```bash
cd build
ctest --test-dir tests -R "ProgressReporter" -V
```

---

## Acceptance Criteria

### Tier 1: Critical (Must Fix)

- [ ] Progress bar updates visibly during execution
- [ ] Thread-safe: works correctly with `--threads N`
- [ ] Works on Windows CMD and PowerShell
- [ ] Updates at least every 500ms during long operations
- [ ] Shows accurate file count (X/N files completed)
- [ ] Console output is properly flushed after each update

### Tier 2: Granularity (Should Have)

- [ ] Shows progress DURING file compression (bytes / total)
- [ ] Updates at regular intervals during long file operations
- [ ] ETA updates as files complete
- [ ] Throughput (MB/s) displayed

### Tier 3: Polish (Nice to Have)

- [ ] Color-coded output (green/yellow on slow operations)
- [ ] Clear "done" message after completion
- [ ] Summary statistics at the end

---

## Performance Considerations

- Mutex contention: minimize lock time in `updateDisplay()`
- Console I/O: limit update frequency to 500ms intervals
- Atomic counters: use `std::atomic` for thread-safe updates
- Memory: no significant overhead

---

## Files to Create/Modify

| File | Action |
|------|--------|
| `src/mosqueeze-bench/src/ProgressReporter.hpp` | Modify |
| `src/mosqueeze-bench/src/ProgressReporter.cpp` | Modify |
| `src/mosqueeze-bench/src/BenchmarkRunner.cpp` | Modify |
| `tests/unit/ProgressReporter_test.cpp` | Modify |
| `CMakeLists.txt` | No changes needed |

---

## Notes

- Windows console may need `SetConsoleMode` for virtual terminal support
- Consider using `std::atomic` for all counters to avoid mutex overhead
- Progress bar should work in both interactive and redirected output
- Test with various terminal widths (narrow/wide consoles)