#pragma once

#include <cstddef>

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
#define MOSQUEEZE_WINDOWS 1
#elif defined(__linux__)
#define MOSQUEEZE_LINUX 1
#elif defined(__APPLE__)
#define MOSQUEEZE_MACOS 1
#endif

namespace mosqueeze::platform {

// Get current process memory usage in bytes
size_t getPeakMemoryUsage();

// Get number of logical CPUs
unsigned int getProcessorCount();

} // namespace mosqueeze::platform
