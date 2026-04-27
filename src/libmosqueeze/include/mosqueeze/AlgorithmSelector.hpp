#pragma once

#include <mosqueeze/FileClassification.hpp>
#include <mosqueeze/FileAnalyzer.hpp>
#include <mosqueeze/ICompressionEngine.hpp>
#include <mosqueeze/Types.hpp>
#include <mosqueeze/bench/BenchmarkResult.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace mosqueeze {

struct Selection {
    std::string algorithm;
    int level = 0;
    bool shouldSkip = false;
    std::string rationale;
    std::string fallbackAlgorithm;
    int fallbackLevel = 0;
};

class AlgorithmSelector {
public:
    AlgorithmSelector();

    Selection select(
        const FileClassification& classification,
        const std::filesystem::path& file = {}) const;

    Selection select(
        const FileClassification& classification,
        const FileFeatures& features) const;

    void setBenchmarkData(const std::vector<bench::BenchmarkResult>& results);
    void clearBenchmarkData();

    void setConfig(const SelectionConfig& config);
    const SelectionConfig& config() const;

    bool loadRules(const std::filesystem::path& configPath);
    bool saveRules(const std::filesystem::path& configPath) const;

    void registerEngine(const ICompressionEngine* engine);
    std::vector<std::string> availableAlgorithms() const;

private:
    struct SelectionRule {
        FileType fileType = FileType::Unknown;
        std::string algorithm;
        int level = 0;
        std::string rationale;
        bool skip = false;
        std::string fallbackAlgorithm;
        int fallbackLevel = 0;
    };

    std::unordered_map<FileType, SelectionRule> typeToRule_;
    std::unordered_map<FileType, std::vector<bench::BenchmarkResult>> benchmarkByFileType_;
    bool hasBenchmarkData_ = false;
    std::unordered_map<std::string, const ICompressionEngine*> engines_;
    SelectionConfig config_;

    void initializeDefaultRules();
    bool shouldSkip(const FileClassification& classification, const FileFeatures& features) const;
    Selection selectFromRules(const FileClassification& classification) const;
    Selection selectFromBenchmark(const FileClassification& classification) const;

    int validateLevel(const std::string& algorithm, int level) const;
};

} // namespace mosqueeze
