#pragma once

#include <mosqueeze/FileAnalyzer.hpp>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace mosqueeze {

enum class OptimizationGoal : unsigned char {
    MinSize = 0,
    Fastest = 1,
    Balanced = 2,
    MinMemory = 3,
    BestDecompression = 4
};

struct QueryResult {
    std::string preprocessor = "none";
    std::string algorithm = "zstd";
    int level = 19;
    double expectedRatio = 0.9;
    double expectedTimeMs = 0.0;
    size_t sampleCount = 0;
    std::string source = "heuristic";
};

class BenchmarkDatabase {
public:
    BenchmarkDatabase() = default;
    ~BenchmarkDatabase();

    BenchmarkDatabase(const BenchmarkDatabase&) = delete;
    BenchmarkDatabase& operator=(const BenchmarkDatabase&) = delete;

    BenchmarkDatabase(BenchmarkDatabase&& other) noexcept;
    BenchmarkDatabase& operator=(BenchmarkDatabase&& other) noexcept;

    bool open(const std::filesystem::path& dbPath);
    void close();
    bool isOpen() const { return db_ != nullptr; }

    std::vector<QueryResult> query(const FileFeatures& features, OptimizationGoal goal, size_t limit) const;

    bool recordResult(
        const std::string& fileKey,
        const FileFeatures& features,
        const std::string& preprocessor,
        const std::string& algorithm,
        int level,
        size_t compressedSize,
        size_t originalSize,
        std::chrono::milliseconds compressionTime);

private:
    sqlite3* db_ = nullptr;

    bool initializeSchema();
    std::vector<QueryResult> queryPatternTable(const FileFeatures& features, OptimizationGoal goal, size_t limit) const;
    std::vector<QueryResult> queryLearningTable(const FileFeatures& features, OptimizationGoal goal, size_t limit) const;
};

} // namespace mosqueeze
