#pragma once

#include <mosqueeze/BenchmarkDatabase.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mosqueeze {

struct Recommendation {
    std::string preprocessor = "none";
    std::string algorithm = "zstd";
    int level = 19;

    double expectedRatio = 0.9;
    double expectedTimeMs = 0.0;
    size_t expectedSize = 0;

    double confidence = 0.5;
    size_t sampleCount = 0;
    std::string explanation;

    std::vector<Recommendation> alternatives;
};

class IntelligentSelector {
public:
    explicit IntelligentSelector(OptimizationGoal goal = OptimizationGoal::MinSize);

    Recommendation analyze(const std::filesystem::path& file);
    Recommendation analyze(std::span<const uint8_t> data, std::string_view filenameHint = "");

    Recommendation analyzeInteractive(const std::filesystem::path& file);
    Recommendation analyzeWithAlternatives(
        const std::filesystem::path& file,
        std::vector<OptimizationGoal> alternativeGoals = {OptimizationGoal::Fastest, OptimizationGoal::Balanced});

    void recordResult(
        const std::filesystem::path& file,
        const std::string& preprocessor,
        const std::string& algorithm,
        int level,
        size_t compressedSize,
        std::chrono::milliseconds compressionTime);

    bool loadBenchmarkDatabase(const std::filesystem::path& dbPath);
    void setOnlineLearning(bool enabled, const std::filesystem::path& userDbPath = {});

    void setOptimizationGoal(OptimizationGoal goal) { goal_ = goal; }
    void setMaxAlternatives(size_t count) { maxAlternatives_ = count; }
    void setConfidenceThreshold(double threshold) { confidenceThreshold_ = threshold; }

private:
    Recommendation analyzeFeatures(const FileFeatures& features);
    Recommendation fromQueryResult(const FileFeatures& features, const QueryResult& result) const;
    Recommendation heuristicFallback(const FileFeatures& features) const;
    std::string generateExplanation(const FileFeatures& features, const Recommendation& rec, std::string_view source) const;
    double calculateConfidence(const FileFeatures& features, const QueryResult& result) const;

    static std::string hashFilePath(const std::filesystem::path& file);

    FileAnalyzer analyzer_{};
    BenchmarkDatabase database_{};
    BenchmarkDatabase userDatabase_{};
    OptimizationGoal goal_ = OptimizationGoal::MinSize;
    size_t maxAlternatives_ = 3;
    double confidenceThreshold_ = 0.7;
    bool onlineLearningEnabled_ = false;
};

} // namespace mosqueeze
