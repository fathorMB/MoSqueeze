#include "Platform.hpp"

#include <thread>

#if defined(MOSQUEEZE_WINDOWS)
#include <windows.h>
#include <psapi.h>
#elif defined(MOSQUEEZE_LINUX) || defined(MOSQUEEZE_MACOS)
#include <sys/resource.h>
#endif

namespace mosqueeze::platform {

size_t getPeakMemoryUsage() {
#if defined(MOSQUEEZE_WINDOWS)
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) == 0) {
        return 0;
    }
    return static_cast<size_t>(counters.PeakWorkingSetSize);
#elif defined(MOSQUEEZE_LINUX) || defined(MOSQUEEZE_MACOS)
    struct rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return 0;
    }
#if defined(MOSQUEEZE_MACOS)
    return static_cast<size_t>(usage.ru_maxrss);
#else
    return static_cast<size_t>(usage.ru_maxrss) * 1024ULL;
#endif
#else
    return 0;
#endif
}

unsigned int getProcessorCount() {
    const auto count = std::thread::hardware_concurrency();
    return count == 0 ? 1 : count;
}

} // namespace mosqueeze::platform
