#include <mosqueeze/bench/BenchmarkConfig.hpp>

#include <cassert>
#include <iostream>

int main() {
    mosqueeze::bench::BenchmarkConfig config;

    assert(config.iterations == 1);
    assert(config.warmupIterations == 0);
    assert(config.trackMemory);
    assert(config.runDecode);
    assert(config.maxTimePerFile.count() == 3600);
    assert(config.threadCount == 0);
    assert(!config.sequential);
    assert(config.preprocessMode == "none");
    assert(!config.usePreprocessing());
    assert(!config.autoPreprocess());
    assert(config.getEffectiveThreadCount() >= 1);
    assert(config.files.empty());
    assert(!config.useStdin);

    config.threadCount = 3;
    assert(config.getEffectiveThreadCount() == 3);
    config.sequential = true;
    assert(config.getEffectiveThreadCount() == 1);
    config.preprocessMode = "auto";
    assert(config.usePreprocessing());
    assert(config.autoPreprocess());
    config.preprocessMode = "image-meta-strip";
    assert(config.usePreprocessing());
    assert(!config.autoPreprocess());

    bool called = false;
    config.onProgress = [&](const mosqueeze::bench::ProgressInfo& info) {
        called = true;
        assert(info.currentLevel == 3);
    };

    mosqueeze::bench::ProgressInfo info{};
    info.currentLevel = 3;
    config.onProgress(info);
    assert(called);

    std::cout << "[PASS] BenchmarkConfig_test\n";
    return 0;
}
