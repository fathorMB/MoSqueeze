# Worker Spec: Fix Progress Bar Duplication

**Issue:** #64
**Branch:** `fix/progress-bar-duplication`
**Type:** Bug Fix - UI/UX

## Problem

Progress bar works correctly for first few minutes, then starts duplicating on new lines instead of updating in-place.

## Root Cause Analysis

### Possible Causes

1. **Thread race condition** - Multiple worker threads calling progress callback simultaneously

2. **Windows CMD buffer limit** - Terminal buffer overflow after many updates

3. **`` handling** - Carriage return not clearing line properly:
   ```cpp
   // Current: may not clear entire line
   std::cout << "\r" << progress_bar << std::flush;
   
   // Should clear to end of line
   std::cout << "\r" << progress_bar << "\033[K" << std::flush;
   ```

4. **Progress callback mutex regression** - #50 fixed mutex bug but may have regressed

## Files to Investigate

```
src/mosqueeze-bench/src/main.cpp         - Progress callback (line ~435)
src/mosqueeze-bench/src/BenchmarkRunner.cpp - Progress reporting
src/libmosqueeze/src/util/Progress.hpp   - Progress class
```

## Implementation

### Step 1: Add Mutex to Progress Callback

```cpp
// src/mosqueeze-bench/src/main.cpp

// BEFORE (buggy):
if (verbose) {
    auto progressMutex = std::make_shared<std::mutex>();
    config.onProgress = [&](const ProgressInfo& info) {
        std::lock_guard<std::mutex> lock(*progressMutex);
        // ... progress output ...
    };
}

// AFTER (fixed):
if (verbose) {
    auto progressMutex = std::make_shared<std::mutex>();
    auto lastUpdate = std::make_shared<std::chrono::steady_clock::time_point>(
        std::chrono::steady_clock::now()
    );
    
    config.onProgress = [progressMutex, lastUpdate](const ProgressInfo& info) {
        std::lock_guard<std::mutex> lock(*progressMutex);
        
        // Throttle updates to max 10 Hz
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - *lastUpdate).count();
        if (elapsed < 100 && info.percent < 100) return;
        *lastUpdate = now;
        
        // Clear line and print progress
        std::cout << "\r\033[K" << formatProgress(info) << std::flush;
    };
}
```

### Step 2: Use ANSI Escape Codes for Windows

Windows 10+ supports ANSI escape codes. Enable them:

```cpp
// At program start (main.cpp)
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
```

### Step 3: Alternative - Use Indicators Library

Consider using a proper progress bar library:

```cpp
// Optional: use indicators for robust progress bars
#include <indicators/progress_bar.hpp>

indicators::ProgressBar bar{
    indicators::option::BarWidth{50},
    indicators::option::Fill{"█"},
    indicators::option::Lead{"█"},
    indicators::option::Remainder{"░"},
};
```

## Testing

```bash
# Run with progress bar
mosqueeze-bench -d "G:\mosqueeze-bench\sources\png" -a zstd --default-only -v

# Run without progress bar (control)
mosqueeze-bench -d "G:\mosqueeze-bench\sources\png" -a zstd --default-only --quiet

# Long run to test durability
mosqueeze-bench -d "large_corpus" -a zstd,brotli,zpaq -v
```

## Acceptance Criteria

- [ ] Progress bar stays in place for runs >30 minutes
- [ ] No duplication on new lines
- [ ] Works on Windows CMD and PowerShell
- [ ] Works with `--threads 4`
- [ ] Progress bar clears properly at 100%

## Related Issues

- #50: Progress bar freeze (mutex fix)
- #58: Preprocessor test coverage

## Estimated Effort

- Investigation: 1-2 hours
- Implementation: 2-3 hours
- Testing: 1 hour
