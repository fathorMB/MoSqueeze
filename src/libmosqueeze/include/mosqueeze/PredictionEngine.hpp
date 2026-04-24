#pragma once

#include <mosqueeze/DecisionMatrix.hpp>
#include <mosqueeze/FileClassification.hpp>
#include <mosqueeze/FileTypeDetector.hpp>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mosqueeze {

using ProgressCallback = std::function<void(const std::string& stage, double progress)>;

struct PredictionConfig {
    bool enablePreprocessing = true;
    bool preferSpeed = false;
    bool includeFallbacks = true;
    size_t fileSizeThreshold = 100 * 1024 * 1024; // 100 MB — warn for slow algos
    std::filesystem::path decisionMatrixPath;       // empty = use embedded
};

struct PredictionResult {
    std::filesystem::path path;
    std::string extension;
    size_t inputSize = 0;
    size_t estimatedOutputSize = 0;

    bool shouldSkip = false;
    std::string skipReason;

    std::string mimeType;
    FileType fileType = FileType::Unknown;

    std::string preprocessor;
    bool preprocessorAvailable = false;

    std::vector<PredictionOption> recommendations;

    const PredictionOption* primary() const;
    const PredictionOption* fastest() const;
    const PredictionOption* balanced() const;

    std::string toJson(int indent = 2) const;
    static PredictionResult fromJson(const std::string& json);
};

class PredictionEngine {
public:
    PredictionEngine();
    ~PredictionEngine();

    void setConfig(const PredictionConfig& config);
    const PredictionConfig& config() const;

    PredictionResult predict(const std::filesystem::path& file,
                             ProgressCallback callback = nullptr) const;

    std::vector<PredictionResult> predictBatch(
        const std::vector<std::filesystem::path>& files,
        ProgressCallback callback = nullptr) const;

    bool hasDataFor(const std::string& extension) const;
    std::vector<std::string> supportedExtensions() const;

    struct Stats {
        size_t totalBenchmarks = 0;
        size_t supportedExtensions = 0;
        std::string dataGenerated;
        std::vector<std::string> skipExtensions;
    };
    Stats getStats() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    std::string determinePreprocessor(const FileClassification& classification) const;
    bool isLargeFile(size_t size) const;
};

} // namespace mosqueeze
