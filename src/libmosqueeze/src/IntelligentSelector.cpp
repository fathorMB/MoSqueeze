#include <mosqueeze/IntelligentSelector.hpp>

#include <algorithm>
#include <functional>
#include <sstream>
#include <utility>

namespace mosqueeze {
namespace {

std::string goalName(OptimizationGoal goal) {
    switch (goal) {
        case OptimizationGoal::Fastest:
            return "fastest";
        case OptimizationGoal::Balanced:
            return "balanced";
        case OptimizationGoal::MinMemory:
            return "min-memory";
        case OptimizationGoal::BestDecompression:
            return "best-decompression";
        case OptimizationGoal::MinSize:
        default:
            return "min-size";
    }
}

} // namespace

IntelligentSelector::IntelligentSelector(OptimizationGoal goal) : goal_(goal) {}

Recommendation IntelligentSelector::analyze(const std::filesystem::path& file) {
    const FileFeatures features = analyzer_.analyze(file);
    return analyzeFeatures(features);
}

Recommendation IntelligentSelector::analyze(std::span<const uint8_t> data, std::string_view filenameHint) {
    const FileFeatures features = analyzer_.analyze(data, filenameHint);
    return analyzeFeatures(features);
}

Recommendation IntelligentSelector::analyzeInteractive(const std::filesystem::path& file) {
    return analyzeWithAlternatives(file, {OptimizationGoal::Fastest, OptimizationGoal::Balanced});
}

Recommendation IntelligentSelector::analyzeWithAlternatives(
    const std::filesystem::path& file,
    std::vector<OptimizationGoal> alternativeGoals) {
    const FileFeatures features = analyzer_.analyze(file);
    Recommendation primary = analyzeFeatures(features);

    primary.alternatives.clear();
    for (const OptimizationGoal altGoal : alternativeGoals) {
        if (primary.alternatives.size() >= maxAlternatives_) {
            break;
        }
        const OptimizationGoal previousGoal = goal_;
        goal_ = altGoal;
        Recommendation alt = analyzeFeatures(features);
        goal_ = previousGoal;

        if (alt.algorithm == primary.algorithm && alt.level == primary.level && alt.preprocessor == primary.preprocessor) {
            continue;
        }
        primary.alternatives.push_back(std::move(alt));
    }
    return primary;
}

void IntelligentSelector::recordResult(
    const std::filesystem::path& file,
    const std::string& preprocessor,
    const std::string& algorithm,
    int level,
    size_t compressedSize,
    std::chrono::milliseconds compressionTime) {
    if (!onlineLearningEnabled_) {
        return;
    }

    FileFeatures features = analyzer_.analyze(file);
    const std::string key = hashFilePath(file);
    (void)userDatabase_.recordResult(
        key,
        features,
        preprocessor,
        algorithm,
        level,
        compressedSize,
        features.fileSize,
        compressionTime);
}

bool IntelligentSelector::loadBenchmarkDatabase(const std::filesystem::path& dbPath) {
    return database_.open(dbPath);
}

void IntelligentSelector::setOnlineLearning(bool enabled, const std::filesystem::path& userDbPath) {
    onlineLearningEnabled_ = enabled;
    if (!enabled) {
        userDatabase_.close();
        return;
    }

    std::filesystem::path path = userDbPath;
    if (path.empty()) {
        path = "benchmarks/results/intelligent-learning.sqlite3";
    }
    (void)userDatabase_.open(path);
}

Recommendation IntelligentSelector::analyzeFeatures(const FileFeatures& features) {
    std::vector<QueryResult> candidates;
    if (database_.isOpen()) {
        auto queryResults = database_.query(features, goal_, maxAlternatives_ + 2);
        candidates.insert(candidates.end(), queryResults.begin(), queryResults.end());
    }
    if (onlineLearningEnabled_ && userDatabase_.isOpen()) {
        auto learned = userDatabase_.query(features, goal_, maxAlternatives_ + 2);
        candidates.insert(candidates.end(), learned.begin(), learned.end());
    }

    if (!candidates.empty()) {
        std::sort(candidates.begin(), candidates.end(), [this](const QueryResult& lhs, const QueryResult& rhs) {
            if (goal_ == OptimizationGoal::Fastest || goal_ == OptimizationGoal::BestDecompression ||
                goal_ == OptimizationGoal::MinMemory) {
                if (lhs.expectedTimeMs != rhs.expectedTimeMs) {
                    return lhs.expectedTimeMs < rhs.expectedTimeMs;
                }
                return lhs.expectedRatio < rhs.expectedRatio;
            }
            if (goal_ == OptimizationGoal::Balanced) {
                const double left = lhs.expectedRatio * 0.75 + (lhs.expectedTimeMs / 10000.0) * 0.25;
                const double right = rhs.expectedRatio * 0.75 + (rhs.expectedTimeMs / 10000.0) * 0.25;
                return left < right;
            }
            if (lhs.expectedRatio != rhs.expectedRatio) {
                return lhs.expectedRatio < rhs.expectedRatio;
            }
            return lhs.expectedTimeMs < rhs.expectedTimeMs;
        });

        Recommendation primary = fromQueryResult(features, candidates.front());
        if (primary.confidence < confidenceThreshold_) {
            Recommendation fallback = heuristicFallback(features);
            fallback.alternatives.push_back(primary);
            return fallback;
        }

        for (size_t i = 1; i < candidates.size() && primary.alternatives.size() < maxAlternatives_; ++i) {
            primary.alternatives.push_back(fromQueryResult(features, candidates[i]));
        }
        return primary;
    }

    return heuristicFallback(features);
}

Recommendation IntelligentSelector::fromQueryResult(const FileFeatures& features, const QueryResult& result) const {
    Recommendation rec{};
    rec.preprocessor = result.preprocessor;
    rec.algorithm = result.algorithm;
    rec.level = result.level;
    rec.expectedRatio = result.expectedRatio;
    rec.expectedTimeMs = result.expectedTimeMs;
    rec.expectedSize = static_cast<size_t>(static_cast<double>(features.fileSize) * result.expectedRatio);
    rec.sampleCount = result.sampleCount;
    rec.confidence = calculateConfidence(features, result);
    rec.explanation = generateExplanation(features, rec, result.source);
    return rec;
}

Recommendation IntelligentSelector::heuristicFallback(const FileFeatures& features) const {
    Recommendation rec{};
    rec.expectedSize = features.fileSize;
    rec.sampleCount = 0;

    if (features.detectedType == "application/json") {
        rec.preprocessor = "json-canonical";
        rec.algorithm = "brotli";
        rec.level = 11;
        rec.expectedRatio = 0.15;
        rec.expectedTimeMs = 250.0;
        rec.confidence = 0.85;
    } else if (features.detectedType == "application/xml" || features.detectedType == "text/xml") {
        rec.preprocessor = "xml-canonical";
        rec.algorithm = "brotli";
        rec.level = 11;
        rec.expectedRatio = 0.20;
        rec.expectedTimeMs = 300.0;
        rec.confidence = 0.80;
    } else if (features.detectedType == "image/png") {
        rec.preprocessor = "png-optimizer";
        rec.algorithm = "zstd";
        rec.level = 19;
        rec.expectedRatio = 0.75;
        rec.expectedTimeMs = 800.0;
        rec.confidence = 0.75;
    } else if (features.rawFormat.has_value()) {
        if (RawFormatDetector::shouldApplyBayerPreprocessor(features.rawFormat->compression)) {
            rec.preprocessor = "bayer-raw";
            rec.algorithm = "zstd";
            rec.level = 19;
            rec.expectedRatio = 0.80;
            rec.expectedTimeMs = 900.0;
            rec.confidence = 0.70;
        } else {
            rec.preprocessor = "none";
            rec.algorithm = "zstd";
            rec.level = 19;
            rec.expectedRatio = 0.88;
            rec.expectedTimeMs = 650.0;
            rec.confidence = 0.90;
        }
    } else if (features.entropy > 7.5) {
        rec.preprocessor = "none";
        rec.algorithm = "zstd";
        rec.level = 19;
        rec.expectedRatio = 0.95;
        rec.expectedTimeMs = 500.0;
        rec.confidence = 0.70;
    } else if (features.isStructured && features.entropy < 6.0) {
        rec.preprocessor = "none";
        rec.algorithm = "brotli";
        rec.level = 11;
        rec.expectedRatio = 0.10;
        rec.expectedTimeMs = 400.0;
        rec.confidence = 0.65;
    } else {
        rec.preprocessor = "none";
        rec.algorithm = "zstd";
        rec.level = 19;
        rec.expectedRatio = 0.85;
        rec.expectedTimeMs = 600.0;
        rec.confidence = 0.50;
    }

    if (goal_ == OptimizationGoal::Fastest) {
        rec.level = 3;
        rec.expectedTimeMs = std::max(40.0, rec.expectedTimeMs * 0.35);
        rec.expectedRatio = std::min(0.98, rec.expectedRatio + 0.05);
    } else if (goal_ == OptimizationGoal::Balanced) {
        rec.level = std::min(rec.level, 9);
        rec.expectedTimeMs *= 0.60;
        rec.expectedRatio = std::min(0.98, rec.expectedRatio + 0.02);
    }

    rec.expectedSize = static_cast<size_t>(static_cast<double>(features.fileSize) * rec.expectedRatio);
    rec.explanation = generateExplanation(features, rec, "heuristic");
    return rec;
}

std::string IntelligentSelector::generateExplanation(
    const FileFeatures& features,
    const Recommendation& rec,
    std::string_view source) const {
    std::ostringstream oss;
    oss << "Goal=" << goalName(goal_) << "; source=" << source << "; type=" << features.detectedType;
    if (features.rawFormat.has_value()) {
        oss << "; raw=" << features.rawFormat->manufacturer << " (" << features.rawFormat->compressionName << ")";
    }
    oss << "; entropy=" << features.entropy;
    oss << "; selected " << rec.preprocessor << " + " << rec.algorithm << ":" << rec.level;
    return oss.str();
}

double IntelligentSelector::calculateConfidence(const FileFeatures& features, const QueryResult& result) const {
    double confidence = 0.45;
    if (result.source == "benchmark") {
        confidence += 0.20;
    } else if (result.source == "learning") {
        confidence += 0.15;
    }
    if (!features.detectedType.empty() && features.detectedType != "application/octet-stream") {
        confidence += 0.10;
    }
    if (features.rawFormat.has_value()) {
        confidence += 0.10;
    }

    const double sampleBoost = std::min(0.20, static_cast<double>(result.sampleCount) / 200.0);
    confidence += sampleBoost;
    if (confidence > 0.99) {
        confidence = 0.99;
    }
    return confidence;
}

std::string IntelligentSelector::hashFilePath(const std::filesystem::path& file) {
    const std::string normalized = file.lexically_normal().string();
    const size_t h = std::hash<std::string>{}(normalized);
    std::ostringstream oss;
    oss << std::hex << h;
    return oss.str();
}

} // namespace mosqueeze
