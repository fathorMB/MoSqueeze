#include <mosqueeze/bench/ProgressReporter.hpp>

#include <cassert>
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

int main() {
    using mosqueeze::bench::ProgressInfo;
    using mosqueeze::bench::ProgressReporter;

    {
        std::ostringstream capture;
        auto* oldBuf = std::cout.rdbuf(capture.rdbuf());

        {
            ProgressReporter reporter(3, false, true);
            ProgressInfo info{};
            info.totalFiles = 3;
            info.completedFiles = 1;
            info.progress = 1.0 / 3.0;
            info.currentFile = "quiet.txt";
            reporter.onProgress(info);
        }

        std::cout.rdbuf(oldBuf);
        assert(capture.str().empty());
    }

    {
        std::ostringstream capture;
        auto* oldBuf = std::cout.rdbuf(capture.rdbuf());

        {
            ProgressReporter reporter(4, true, false);

            ProgressInfo start{};
            start.totalFiles = 4;
            start.completedFiles = 0;
            start.progress = 0.0;
            start.currentFile = "alpha.bin";
            start.currentAlgorithm = "zstd";
            start.currentLevel = 22;
            start.currentIteration = 1;
            start.totalIterations = 2;
            reporter.onProgress(start);

            std::this_thread::sleep_for(std::chrono::milliseconds(120));

            ProgressInfo mid{};
            mid.totalFiles = 4;
            mid.completedFiles = 1;
            mid.progress = 0.25;
            mid.currentFile = "alpha.bin";
            mid.currentAlgorithm = "zstd";
            mid.currentLevel = 22;
            mid.currentIteration = 2;
            mid.totalIterations = 2;
            reporter.onProgress(mid);
        }

        std::cout.rdbuf(oldBuf);
        const std::string text = capture.str();
        assert(text.find("Progress [") != std::string::npos);
        assert(text.find("1/4") != std::string::npos);
        assert(text.find("alpha.bin") != std::string::npos);
        assert(text.find("zstd L22") != std::string::npos);
        assert(text.find("iter 2/2") != std::string::npos);
        assert(text.find("ETA ") != std::string::npos);
    }

    {
        ProgressReporter reporter(100, false, true);
        std::vector<std::thread> workers;
        workers.reserve(8);
        for (int t = 0; t < 8; ++t) {
            workers.emplace_back([&reporter, t]() {
                for (int i = 0; i < 20; ++i) {
                    ProgressInfo info{};
                    info.totalFiles = 100;
                    info.completedFiles = t * 10 + i;
                    info.progress = static_cast<double>(info.completedFiles) / 100.0;
                    info.currentFile = "threaded.bin";
                    info.currentAlgorithm = "zstd";
                    info.currentLevel = 3;
                    reporter.onProgress(info);
                }
            });
        }
        for (auto& worker : workers) {
            worker.join();
        }
    }

    std::cout << "[PASS] ProgressReporter_test\n";
    return 0;
}
