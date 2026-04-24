#pragma once

#include <mosqueeze/bench/BenchmarkResult.hpp>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mosqueeze {

enum class CompressionSpeed {
    Fast,   // < 100ms
    Medium, // 100ms - 1s
    Slow    // > 1s
};

struct PredictionOption {
    std::string algorithm;
    int level = 0;
    double estimatedRatio = 1.0;
    size_t estimatedSize = 0;
    CompressionSpeed speed = CompressionSpeed::Slow;
    std::string speedLabel;

    std::string fallbackAlgorithm;
    int fallbackLevel = 0;
    double fallbackRatio = 1.0;
};

struct Prediction {
    bool shouldSkip = false;
    std::string skipReason;
    std::string fileExtension;
    FileType fileType = FileType::Unknown;
    std::vector<PredictionOption> options;

    const PredictionOption* getBest() const;
    const PredictionOption* getFastest() const;
    const PredictionOption* getBalanced() const;
};

class DecisionMatrix {
public:
    DecisionMatrix();

    bool loadDefault();
    bool loadFromFile(const std::filesystem::path& path);

    Prediction predict(const std::string& extension, size_t fileSize = 0) const;
    Prediction predict(const std::filesystem::path& file) const;

    bool hasData(const std::string& extension) const;
    std::vector<std::string> knownExtensions() const;

    size_t totalEntries() const;
    size_t totalBenchmarks() const;

private:
    struct ExtensionData {
        bool skip = false;
        std::string skipReason;
        size_t fileCount = 0;
        size_t testCount = 0;
        double avgRatio = 1.0;

        std::string bestRatioAlgo;
        int bestRatioLevel = 0;
        double bestRatio = 1.0;

        std::string bestFastAlgo;
        int bestFastLevel = 0;
        double bestFastRatio = 1.0;

        std::string bestBalancedAlgo;
        int bestBalancedLevel = 0;
        double bestBalancedRatio = 1.0;

        std::vector<bench::BenchmarkResult> results;
    };

    std::unordered_map<std::string, ExtensionData> data_;
    std::string generatedDate_;
    size_t totalBenchmarks_ = 0;

    bool parseJsonContent(std::string_view content);
    CompressionSpeed classifySpeed(std::chrono::milliseconds encodeTime) const;
    std::string fileExtensionToLower(const std::filesystem::path& path) const;
    Prediction buildPrediction(const std::string& ext, const ExtensionData& ed, size_t fileSize) const;
    static Prediction buildFallback(const std::string& ext, size_t fileSize);
};

} // namespace mosqueeze
