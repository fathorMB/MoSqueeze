#include <mosqueeze/PredictionEngine.hpp>

#include <mosqueeze/AlgorithmSelector.hpp>
#include <mosqueeze/DecisionMatrix.hpp>
#include <mosqueeze/FileTypeDetector.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace mosqueeze {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

class PredictionEngine::Impl {
public:
    PredictionConfig config;
    DecisionMatrix   decisionMatrix;
    FileTypeDetector typeDetector;

    bool initialize() {
        if (!config.decisionMatrixPath.empty())
            return decisionMatrix.loadFromFile(config.decisionMatrixPath);
        return decisionMatrix.loadDefault();
    }
};

// ---------------------------------------------------------------------------
// PredictionResult accessors
// ---------------------------------------------------------------------------

const PredictionOption* PredictionResult::primary() const {
    return recommendations.empty() ? nullptr : &recommendations[0];
}

const PredictionOption* PredictionResult::fastest() const {
    if (recommendations.size() > 1) return &recommendations[1];
    return recommendations.empty() ? nullptr : &recommendations[0];
}

const PredictionOption* PredictionResult::balanced() const {
    if (recommendations.size() > 2) return &recommendations[2];
    if (recommendations.size() > 1) return &recommendations[1];
    return recommendations.empty() ? nullptr : &recommendations[0];
}

// ---------------------------------------------------------------------------
// PredictionResult serialisation
// ---------------------------------------------------------------------------

std::string PredictionResult::toJson(int indent) const {
    nlohmann::json j;
    j["file"]                  = path.string();
    j["extension"]             = extension;
    j["input_size"]            = inputSize;
    j["estimated_output_size"] = estimatedOutputSize;
    j["should_skip"]           = shouldSkip;
    j["mime_type"]             = mimeType;
    j["preprocessor"]          = preprocessor;
    j["preprocessor_available"]= preprocessorAvailable;

    if (shouldSkip)
        j["skip_reason"] = skipReason;

    j["recommendations"] = nlohmann::json::array();
    for (const auto& opt : recommendations) {
        nlohmann::json o;
        o["algorithm"]       = opt.algorithm;
        o["level"]           = opt.level;
        o["estimated_ratio"] = opt.estimatedRatio;
        o["estimated_size"]  = opt.estimatedSize;
        o["speed"]           = opt.speedLabel;
        if (!opt.fallbackAlgorithm.empty()) {
            o["fallback_algorithm"] = opt.fallbackAlgorithm;
            o["fallback_level"]     = opt.fallbackLevel;
            o["fallback_ratio"]     = opt.fallbackRatio;
        }
        j["recommendations"].push_back(std::move(o));
    }

    return j.dump(indent);
}

static CompressionSpeed speedFromLabel(const std::string& label) {
    if (label == "fast")   return CompressionSpeed::Fast;
    if (label == "medium") return CompressionSpeed::Medium;
    return CompressionSpeed::Slow;
}

PredictionResult PredictionResult::fromJson(const std::string& json) {
    const auto j = nlohmann::json::parse(json);

    PredictionResult r;
    r.path                  = j.value("file", std::string{});
    r.extension             = j.value("extension", std::string{});
    r.inputSize             = j.value("input_size", size_t{0});
    r.estimatedOutputSize   = j.value("estimated_output_size", size_t{0});
    r.shouldSkip            = j.value("should_skip", false);
    r.skipReason            = j.value("skip_reason", std::string{});
    r.mimeType              = j.value("mime_type", std::string{});
    r.preprocessor          = j.value("preprocessor", std::string{});
    r.preprocessorAvailable = j.value("preprocessor_available", false);

    if (j.contains("recommendations")) {
        for (const auto& o : j["recommendations"]) {
            PredictionOption opt;
            opt.algorithm      = o.value("algorithm", std::string{});
            opt.level          = o.value("level", 0);
            opt.estimatedRatio = o.value("estimated_ratio", 1.0);
            opt.estimatedSize  = o.value("estimated_size", size_t{0});
            const std::string sl = o.value("speed", std::string{"slow"});
            opt.speed          = speedFromLabel(sl);
            opt.speedLabel     = sl;
            opt.fallbackAlgorithm = o.value("fallback_algorithm", std::string{});
            opt.fallbackLevel     = o.value("fallback_level", 0);
            opt.fallbackRatio     = o.value("fallback_ratio", 1.0);
            r.recommendations.push_back(std::move(opt));
        }
    }

    return r;
}

// ---------------------------------------------------------------------------
// PredictionEngine
// ---------------------------------------------------------------------------

PredictionEngine::PredictionEngine()
    : impl_(std::make_unique<Impl>()) {
    impl_->initialize();
}

PredictionEngine::~PredictionEngine() = default;

void PredictionEngine::setConfig(const PredictionConfig& cfg) {
    impl_->config = cfg;
    impl_->initialize();
}

const PredictionConfig& PredictionEngine::config() const {
    return impl_->config;
}

std::string PredictionEngine::determinePreprocessor(
    const FileClassification& c) const
{
    if (c.mimeType == "application/json" || c.extension == ".json")
        return "json-canonical";
    if (c.mimeType == "application/xml" || c.mimeType == "text/xml" ||
        c.extension == ".xml")
        return "xml-canonical";
    if (c.type == FileType::Image_Raw)
        return "bayer-raw";
    return "none";
}

bool PredictionEngine::isLargeFile(size_t size) const {
    return size >= impl_->config.fileSizeThreshold;
}

PredictionResult PredictionEngine::predict(
    const std::filesystem::path& file,
    ProgressCallback callback) const
{
    PredictionResult result;
    result.path = file;

    // File size
    {
        std::error_code ec;
        if (std::filesystem::exists(file, ec) && !ec) {
            auto sz = std::filesystem::file_size(file, ec);
            if (!ec) result.inputSize = sz;
        }
    }

    // Extension (lower-case)
    result.extension = file.extension().string();
    std::transform(result.extension.begin(), result.extension.end(),
                   result.extension.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // File-type classification
    if (callback) callback("classifying", 0.1);
    const FileClassification classification =
        impl_->typeDetector.detect(file);
    result.mimeType = classification.mimeType;
    result.fileType = classification.type;

    // Type-detector-driven skip (e.g. JPEG magic bytes → isCompressed)
    if (!classification.canRecompress ||
        classification.recommendation == "skip") {
        if (callback) callback("skip", 1.0);
        result.shouldSkip = true;
        result.skipReason = result.mimeType +
            " is already compressed or not suitable for re-compression";
        return result;
    }

    // Preprocessor
    if (impl_->config.enablePreprocessing) {
        if (callback) callback("preprocessing", 0.3);
        result.preprocessor = determinePreprocessor(classification);
        result.preprocessorAvailable =
            !result.preprocessor.empty() && result.preprocessor != "none";
    } else {
        result.preprocessor = "none";
    }

    // Decision-matrix prediction
    if (callback) callback("predicting", 0.5);
    auto prediction =
        impl_->decisionMatrix.predict(result.extension, result.inputSize);

    result.shouldSkip    = prediction.shouldSkip;
    result.skipReason    = prediction.skipReason;
    result.recommendations = std::move(prediction.options);

    if (result.shouldSkip) {
        if (callback) callback("complete", 1.0);
        return result;
    }

    // Recompute estimated sizes (already done inside DecisionMatrix, but
    // ensure consistency after any sorting below).
    if (result.inputSize > 0) {
        for (auto& opt : result.recommendations) {
            if (opt.estimatedRatio > 0.0)
                opt.estimatedSize =
                    static_cast<size_t>(result.inputSize / opt.estimatedRatio);
        }
    }

    // Update overall estimated output from the primary recommendation
    if (const auto* p = result.primary())
        result.estimatedOutputSize = p->estimatedSize;

    // Re-sort when speed is preferred (fast first, then by ratio within tier)
    if (impl_->config.preferSpeed && !result.recommendations.empty()) {
        std::stable_sort(result.recommendations.begin(),
                         result.recommendations.end(),
                         [](const PredictionOption& a, const PredictionOption& b) {
                             if (a.speed != b.speed)
                                 return static_cast<int>(a.speed) <
                                        static_cast<int>(b.speed);
                             return a.estimatedRatio > b.estimatedRatio;
                         });
        // Refresh estimated output after reorder
        if (const auto* p = result.primary())
            result.estimatedOutputSize = p->estimatedSize;
    }

    if (callback) callback("complete", 1.0);
    return result;
}

std::vector<PredictionResult> PredictionEngine::predictBatch(
    const std::vector<std::filesystem::path>& files,
    ProgressCallback callback) const
{
    std::vector<PredictionResult> results;
    results.reserve(files.size());

    const double total = static_cast<double>(files.size());
    for (size_t i = 0; i < files.size(); ++i) {
        if (callback)
            callback("batch", static_cast<double>(i) / total);
        results.push_back(predict(files[i]));
    }

    if (callback) callback("batch", 1.0);
    return results;
}

bool PredictionEngine::hasDataFor(const std::string& extension) const {
    return impl_->decisionMatrix.hasData(extension);
}

std::vector<std::string> PredictionEngine::supportedExtensions() const {
    return impl_->decisionMatrix.knownExtensions();
}

PredictionEngine::Stats PredictionEngine::getStats() const {
    Stats s;
    s.totalBenchmarks    = impl_->decisionMatrix.totalBenchmarks();
    s.supportedExtensions = impl_->decisionMatrix.totalEntries();

    for (const auto& ext : impl_->decisionMatrix.knownExtensions()) {
        const auto p = impl_->decisionMatrix.predict(ext);
        if (p.shouldSkip)
            s.skipExtensions.push_back(ext);
    }
    return s;
}

} // namespace mosqueeze
