#pragma once

#include <mosqueeze/ICompressionEngine.hpp>
#include <mosqueeze/bench/BenchmarkConfig.hpp>
#include <mosqueeze/bench/BenchmarkResult.hpp>

#include <atomic>
#include <filesystem>
#include <mutex>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mosqueeze::bench {

struct AlgorithmConstraint {
    size_t maxFileSize = 0;    // 0 = unlimited
    int maxLevelForSize = 0;  // For files > maxFileSize, limit to this level
};

class BenchmarkRunner {
public:
    BenchmarkRunner();
    ~BenchmarkRunner();

    void registerEngine(std::unique_ptr<ICompressionEngine> engine);
    void registerConstraint(const std::string& algorithm, AlgorithmConstraint constraint);
    std::vector<std::string> availableAlgorithms() const;
    std::vector<int> availableLevels(const std::string& algorithm) const;
    std::vector<int> filterLevelsBySize(
        const std::string& algorithm,
        const std::vector<int>& levels,
        size_t fileSize) const;

    std::vector<BenchmarkResult> run(
        const std::vector<std::filesystem::path>& files,
        const std::vector<std::string>& algorithms = {},
        const std::vector<int>& levels = {});

    std::vector<BenchmarkResult> runGrid(
        const std::vector<std::filesystem::path>& files);

    std::vector<BenchmarkResult> runWithConfig(const BenchmarkConfig& config);
    std::vector<BenchmarkResult> runParallel(const BenchmarkConfig& config);

    std::unordered_map<std::string, BenchmarkStats> computeStats(
        const std::vector<BenchmarkResult>& results) const;

private:
    std::vector<std::unique_ptr<ICompressionEngine>> engines_;
    std::unordered_map<std::string, AlgorithmConstraint> constraints_;
    ICompressionEngine* findEngine(const std::string& name) const;
    BenchmarkResult runIteration(
        ICompressionEngine* engine,
        const std::filesystem::path& file,
        int level,
        const BenchmarkConfig& config,
        FileType fileType) const;
    void processFile(
        const std::filesystem::path& file,
        const std::vector<ICompressionEngine*>& engines,
        const BenchmarkConfig& config,
        std::vector<BenchmarkResult>& results,
        std::mutex& resultsMutex,
        std::atomic<int>& completedCount,
        std::atomic<size_t>& completedBytes) const;
};

} // namespace mosqueeze::bench
