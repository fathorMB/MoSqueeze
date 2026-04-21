#include <mosqueeze/AlgorithmSelector.hpp>

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace mosqueeze {

AlgorithmSelector::AlgorithmSelector() {
    initializeDefaultRules();
}

void AlgorithmSelector::initializeDefaultRules() {
    typeToRule_.clear();

    typeToRule_[FileType::Text_SourceCode] = {
        FileType::Text_SourceCode,
        "zstd", 22, "Source code: zstd level 22 for best cold storage ratio",
        false, "brotli", 11
    };

    typeToRule_[FileType::Text_Structured] = {
        FileType::Text_Structured,
        "brotli", 11, "JSON/XML/YAML: brotli quality 11 for web content",
        false, "zstd", 22
    };

    typeToRule_[FileType::Text_Prose] = {
        FileType::Text_Prose,
        "brotli", 11, "Plain text/Markdown: brotli for natural language",
        false, "zstd", 22
    };

    typeToRule_[FileType::Text_Log] = {
        FileType::Text_Log,
        "lzma", 9, "Log files: LZMA level 9 for repetitive patterns",
        false, "zstd", 22
    };

    typeToRule_[FileType::Image_PNG] = {
        FileType::Image_PNG,
        "zstd", 19, "PNG: zstd can improve on zlib compression",
        false, "", 0
    };

    typeToRule_[FileType::Image_JPEG] = {
        FileType::Image_JPEG,
        "", 0, "JPEG: already lossy compressed, skip to avoid quality loss",
        true, "", 0
    };

    typeToRule_[FileType::Image_WebP] = {
        FileType::Image_WebP,
        "", 0, "WebP: already compressed, skip",
        true, "", 0
    };

    typeToRule_[FileType::Image_Raw] = {
        FileType::Image_Raw,
        "lzma", 9, "Raw image (BMP/TIFF): LZMA for pixel data",
        false, "zstd", 22
    };

    typeToRule_[FileType::Video_MP4] = {
        FileType::Video_MP4,
        "", 0, "MP4: H.264/265 already max compressed (ratio <5% from ZPAQ)",
        true, "", 0
    };

    typeToRule_[FileType::Video_MKV] = {
        FileType::Video_MKV,
        "zpaq", 5, "MKV: ZPAQ level 5 for cold storage (extracts additional ~10-15%)",
        false, "zstd", 22
    };

    typeToRule_[FileType::Video_WebM] = {
        FileType::Video_WebM,
        "", 0, "WebM: VP9/AV1 max compressed (ratio <5% from ZPAQ)",
        true, "", 0
    };

    typeToRule_[FileType::Video_AVI] = {
        FileType::Video_AVI,
        "zpaq", 5, "AVI: ZPAQ level 5 for maximum ratio (handles raw and compressed)",
        false, "lzma", 9
    };

    typeToRule_[FileType::Audio_WAV] = {
        FileType::Audio_WAV,
        "lzma", 9, "WAV: uncompressed PCM, LZMA works well",
        false, "zstd", 22
    };

    typeToRule_[FileType::Audio_MP3] = {
        FileType::Audio_MP3,
        "", 0, "MP3: already lossy compressed, skip",
        true, "", 0
    };

    typeToRule_[FileType::Audio_FLAC] = {
        FileType::Audio_FLAC,
        "", 0, "FLAC: already lossless compressed, skip",
        true, "", 0
    };

    typeToRule_[FileType::Binary_Executable] = {
        FileType::Binary_Executable,
        "lzma", 9, "ELF/EXE: LZMA excels on structured binary",
        false, "zstd", 22
    };

    typeToRule_[FileType::Binary_Database] = {
        FileType::Binary_Database,
        "zpaq", 5, "Database: ZPAQ level 5 for maximum cold storage ratio",
        false, "lzma", 9
    };

    typeToRule_[FileType::Binary_Chunked] = {
        FileType::Binary_Chunked,
        "zstd", 22, "Generic binary: zstd as safe default",
        false, "lzma", 9
    };

    typeToRule_[FileType::Archive_ZIP] = {
        FileType::Archive_ZIP,
        "", 0, "ZIP: extract and recompress contents individually",
        true, "", 0
    };

    typeToRule_[FileType::Archive_TAR] = {
        FileType::Archive_TAR,
        "zstd", 22, "TAR: uncompressed, use zstd",
        false, "lzma", 9
    };

    typeToRule_[FileType::Archive_7Z] = {
        FileType::Archive_7Z,
        "", 0, "7Z: already LZMA compressed, extract to recompress",
        true, "", 0
    };

    typeToRule_[FileType::Unknown] = {
        FileType::Unknown,
        "zstd", 22, "Unknown: zstd as safe general-purpose default",
        false, "lzma", 9
    };
}

Selection AlgorithmSelector::select(
    const FileClassification& classification,
    const std::filesystem::path& file) const {
    (void)file;

    if (!classification.canRecompress || classification.recommendation == "skip") {
        Selection skipSelection;
        skipSelection.shouldSkip = true;
        skipSelection.rationale = "File type marked as skip: " + classification.mimeType;
        return skipSelection;
    }

    if (classification.recommendation == "extract-then-compress") {
        Selection extractSelection;
        extractSelection.shouldSkip = true;
        extractSelection.rationale = "Archive detected: extract contents first, then compress individually";
        return extractSelection;
    }

    if (hasBenchmarkData_) {
        Selection benchmarkSelection = selectFromBenchmark(classification);
        if (!benchmarkSelection.algorithm.empty()) {
            benchmarkSelection.level = validateLevel(benchmarkSelection.algorithm, benchmarkSelection.level);
            if (!benchmarkSelection.fallbackAlgorithm.empty()) {
                benchmarkSelection.fallbackLevel = validateLevel(
                    benchmarkSelection.fallbackAlgorithm,
                    benchmarkSelection.fallbackLevel);
            }
            return benchmarkSelection;
        }
    }

    Selection ruleSelection = selectFromRules(classification);
    if (!ruleSelection.algorithm.empty()) {
        ruleSelection.level = validateLevel(ruleSelection.algorithm, ruleSelection.level);
    }
    if (!ruleSelection.fallbackAlgorithm.empty()) {
        ruleSelection.fallbackLevel = validateLevel(ruleSelection.fallbackAlgorithm, ruleSelection.fallbackLevel);
    }

    return ruleSelection;
}

Selection AlgorithmSelector::selectFromRules(const FileClassification& classification) const {
    const auto it = typeToRule_.find(classification.type);
    if (it == typeToRule_.end()) {
        Selection fallback;
        fallback.algorithm = "zstd";
        fallback.level = 22;
        fallback.rationale = "Unknown file type: using zstd as safe default";
        fallback.fallbackAlgorithm = "lzma";
        fallback.fallbackLevel = 9;
        return fallback;
    }

    const SelectionRule& rule = it->second;

    Selection selection;
    selection.algorithm = rule.algorithm;
    selection.level = rule.level;
    selection.shouldSkip = rule.skip;
    selection.rationale = rule.rationale;
    selection.fallbackAlgorithm = rule.fallbackAlgorithm;
    selection.fallbackLevel = rule.fallbackLevel;
    return selection;
}

Selection AlgorithmSelector::selectFromBenchmark(const FileClassification& classification) const {
    const auto it = benchmarkByFileType_.find(classification.type);
    if (it == benchmarkByFileType_.end() || it->second.empty()) {
        return {};
    }

    const auto& results = it->second;

    auto best = std::max_element(
        results.begin(),
        results.end(),
        [](const bench::BenchmarkResult& a, const bench::BenchmarkResult& b) {
            return a.ratio() < b.ratio();
        });

    if (best == results.end()) {
        return {};
    }

    Selection selection;
    selection.algorithm = best->algorithm;
    selection.level = best->level;
    selection.rationale =
        "Benchmark-optimized: " + best->algorithm + " achieved " +
        std::to_string(best->ratio()) + "x ratio on similar files";

    if (results.size() > 1) {
        const auto second = std::max_element(
            results.begin(),
            results.end(),
            [best](const bench::BenchmarkResult& a, const bench::BenchmarkResult& b) {
                if (&a == &*best) {
                    return true;
                }
                if (&b == &*best) {
                    return false;
                }
                return a.ratio() < b.ratio();
            });

        if (second != results.end() && second != best) {
            selection.fallbackAlgorithm = second->algorithm;
            selection.fallbackLevel = second->level;
        }
    }

    return selection;
}

int AlgorithmSelector::validateLevel(const std::string& algorithm, int level) const {
    const auto it = engines_.find(algorithm);
    if (it == engines_.end() || it->second == nullptr) {
        return level;
    }

    const ICompressionEngine* engine = it->second;
    const auto levels = engine->supportedLevels();
    if (std::find(levels.begin(), levels.end(), level) != levels.end()) {
        return level;
    }

    return engine->defaultLevel();
}

void AlgorithmSelector::setBenchmarkData(const std::vector<bench::BenchmarkResult>& results) {
    benchmarkByFileType_.clear();
    for (const auto& result : results) {
        benchmarkByFileType_[result.fileType].push_back(result);
    }

    hasBenchmarkData_ = !benchmarkByFileType_.empty();
}

void AlgorithmSelector::clearBenchmarkData() {
    benchmarkByFileType_.clear();
    hasBenchmarkData_ = false;
}

bool AlgorithmSelector::loadRules(const std::filesystem::path& configPath) {
    std::ifstream config(configPath);
    if (!config) {
        return false;
    }

    try {
        nlohmann::json payload;
        config >> payload;

        if (!payload.contains("rules") || !payload["rules"].is_array()) {
            return false;
        }

        for (const auto& ruleJson : payload["rules"]) {
            SelectionRule rule;
            rule.fileType = static_cast<FileType>(ruleJson.value("fileType", static_cast<int>(FileType::Unknown)));
            rule.algorithm = ruleJson.value("algorithm", "");
            rule.level = ruleJson.value("level", 0);
            rule.rationale = ruleJson.value("rationale", "");
            rule.skip = ruleJson.value("skip", false);
            rule.fallbackAlgorithm = ruleJson.value("fallbackAlgorithm", "");
            rule.fallbackLevel = ruleJson.value("fallbackLevel", 0);

            typeToRule_[rule.fileType] = std::move(rule);
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool AlgorithmSelector::saveRules(const std::filesystem::path& configPath) const {
    std::ofstream config(configPath);
    if (!config) {
        return false;
    }

    nlohmann::json payload;
    payload["version"] = 1;
    payload["rules"] = nlohmann::json::array();

    for (const auto& [type, rule] : typeToRule_) {
        payload["rules"].push_back({
            {"fileType", static_cast<int>(type)},
            {"algorithm", rule.algorithm},
            {"level", rule.level},
            {"rationale", rule.rationale},
            {"skip", rule.skip},
            {"fallbackAlgorithm", rule.fallbackAlgorithm},
            {"fallbackLevel", rule.fallbackLevel}
        });
    }

    config << payload.dump(2);
    return true;
}

void AlgorithmSelector::registerEngine(const ICompressionEngine* engine) {
    if (engine != nullptr) {
        engines_[engine->name()] = engine;
    }
}

std::vector<std::string> AlgorithmSelector::availableAlgorithms() const {
    std::vector<std::string> names;
    names.reserve(engines_.size());

    for (const auto& [name, engine] : engines_) {
        (void)engine;
        names.push_back(name);
    }

    std::sort(names.begin(), names.end());
    return names;
}

} // namespace mosqueeze
