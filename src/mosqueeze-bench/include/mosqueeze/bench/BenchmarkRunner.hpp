#pragma once

#include <mosqueeze/ICompressionEngine.hpp>
#include <mosqueeze/bench/BenchmarkResult.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace mosqueeze::bench {

class BenchmarkRunner {
public:
    BenchmarkRunner();

    void registerEngine(std::unique_ptr<ICompressionEngine> engine);

    std::vector<BenchmarkResult> run(
        const std::vector<std::filesystem::path>& files,
        const std::vector<std::string>& algorithms = {},
        const std::vector<int>& levels = {});

    std::vector<BenchmarkResult> runGrid(
        const std::vector<std::filesystem::path>& files);

private:
    std::vector<std::unique_ptr<ICompressionEngine>> engines_;
    ICompressionEngine* findEngine(const std::string& name) const;
};

} // namespace mosqueeze::bench
