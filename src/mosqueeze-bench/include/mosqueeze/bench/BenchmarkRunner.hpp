#pragma once

#include <mosqueeze/ICompressionEngine.hpp>
#include <mosqueeze/Types.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace mosqueeze::bench {

struct BenchmarkResult {
    std::string algorithm;
    int level = 0;
    std::filesystem::path file;
    FileType fileType = FileType::Unknown;
    size_t originalBytes = 0;
    size_t compressedBytes = 0;
    std::chrono::milliseconds encodeTime{0};
    std::chrono::milliseconds decodeTime{0};
    size_t peakMemoryBytes = 0;

    double ratio() const {
        return compressedBytes > 0
            ? static_cast<double>(originalBytes) / compressedBytes
            : 0.0;
    }
};

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
