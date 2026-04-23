#include <mosqueeze/bench/BenchmarkRunner.hpp>
#include <mosqueeze/bench/ThreadPool.hpp>

#include <mosqueeze/FileTypeDetector.hpp>
#include <mosqueeze/PreprocessorSelector.hpp>
#include <mosqueeze/preprocessors/BayerPreprocessor.hpp>
#include <mosqueeze/preprocessors/ImageMetaStripper.hpp>
#include <mosqueeze/preprocessors/JsonCanonicalizer.hpp>
#include <mosqueeze/preprocessors/PngOptimizer.hpp>
#include <mosqueeze/preprocessors/XmlCanonicalizer.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <future>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace mosqueeze::bench {
namespace {

double variance(const std::vector<double>& values, double mean) {
    if (values.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (double value : values) {
        const double delta = value - mean;
        sum += delta * delta;
    }
    return sum / static_cast<double>(values.size());
}

std::vector<ICompressionEngine*> selectEngines(
    const std::vector<std::unique_ptr<ICompressionEngine>>& engines,
    const BenchmarkConfig& config) {
    std::vector<ICompressionEngine*> selectedEngines;
    if (config.allEngines || config.algorithms.empty()) {
        for (const auto& engine : engines) {
            selectedEngines.push_back(engine.get());
        }
    } else {
        std::unordered_set<std::string> selectedAlgorithms(config.algorithms.begin(), config.algorithms.end());
        for (const auto& engine : engines) {
            if (selectedAlgorithms.count(engine->name()) > 0) {
                selectedEngines.push_back(engine.get());
            }
        }
    }
    return selectedEngines;
}

std::unordered_map<std::string, std::vector<int>> buildLevelMap(
    const std::vector<ICompressionEngine*>& engines,
    const BenchmarkConfig& config) {
    std::unordered_map<std::string, std::vector<int>> levelsByAlgorithm;
    for (auto* engine : engines) {
        std::vector<int> levelSet;
        if (config.defaultOnly) {
            levelSet = {engine->defaultLevel()};
        } else if (config.levels.empty()) {
            levelSet = engine->supportedLevels();
        } else {
            const auto supported = engine->supportedLevels();
            for (int level : config.levels) {
                if (std::find(supported.begin(), supported.end(), level) != supported.end()) {
                    levelSet.push_back(level);
                }
            }
            if (levelSet.empty()) {
                levelSet.push_back(engine->defaultLevel());
            }
        }
        std::sort(levelSet.begin(), levelSet.end());
        levelSet.erase(std::unique(levelSet.begin(), levelSet.end()), levelSet.end());
        levelsByAlgorithm[engine->name()] = std::move(levelSet);
    }
    return levelsByAlgorithm;
}

std::unique_ptr<IPreprocessor> createPreprocessor(const std::string& name, const BenchmarkConfig& config) {
    if (name == "bayer-raw") {
        return std::make_unique<BayerPreprocessor>(config.forceBayer);
    }
    if (name == "image-meta-strip") {
        return std::make_unique<ImageMetaStripper>();
    }
    if (name == "json-canonical") {
        return std::make_unique<JsonCanonicalizer>();
    }
    if (name == "xml-canonical") {
        return std::make_unique<XmlCanonicalizer>();
    }
    if (name == "png-optimizer") {
        auto optimizer = std::make_unique<PngOptimizer>(
            config.pngEngine == "oxipng" ? PngEngine::Oxipng : PngEngine::LibPng);
        optimizer->setCompressionLevel(config.pngLevel);
        optimizer->setStripMetadata(config.pngStripMetadata);
        optimizer->setFilterSelection(config.pngAllFilters);
        return optimizer;
    }
    return nullptr;
}

} // namespace

BenchmarkRunner::BenchmarkRunner() = default;
BenchmarkRunner::~BenchmarkRunner() = default;

void BenchmarkRunner::registerEngine(std::unique_ptr<ICompressionEngine> engine) {
    if (!engine) {
        throw std::runtime_error("Cannot register null compression engine");
    }
    engines_.push_back(std::move(engine));
}

std::vector<std::string> BenchmarkRunner::availableAlgorithms() const {
    std::vector<std::string> names;
    names.reserve(engines_.size());
    for (const auto& engine : engines_) {
        names.push_back(engine->name());
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<int> BenchmarkRunner::availableLevels(const std::string& algorithm) const {
    auto* engine = findEngine(algorithm);
    if (!engine) {
        return {};
    }
    auto levels = engine->supportedLevels();
    std::sort(levels.begin(), levels.end());
    return levels;
}

ICompressionEngine* BenchmarkRunner::findEngine(const std::string& name) const {
    auto it = std::find_if(engines_.begin(), engines_.end(), [&](const auto& engine) {
        return engine->name() == name;
    });
    return it == engines_.end() ? nullptr : it->get();
}

BenchmarkResult BenchmarkRunner::runIteration(
    ICompressionEngine* engine,
    const std::filesystem::path& file,
    int level,
    const BenchmarkConfig& config,
    FileType fileType) const {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open file for benchmark: " + file.string());
    }

    std::string rawContent((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string contentToCompress = rawContent;
    PreprocessMetrics preprocessMetrics{};

    if (config.usePreprocessing()) {
        preprocessMetrics.type = config.autoPreprocess() ? "none" : config.preprocessMode;
        preprocessMetrics.originalBytes = rawContent.size();
        preprocessMetrics.processedBytes = rawContent.size();

        std::unique_ptr<IPreprocessor> selectedByName;
        std::unique_ptr<IPreprocessor> selectedAutoOwned;
        std::unique_ptr<PreprocessorSelector> selector;
        IPreprocessor* preprocessor = nullptr;
        if (config.autoPreprocess()) {
            selector = std::make_unique<PreprocessorSelector>();
            preprocessor = selector->selectBest(fileType);
            if (preprocessor != nullptr && preprocessor->name() == "bayer-raw") {
                selectedAutoOwned = createPreprocessor("bayer-raw", config);
            } else if (preprocessor != nullptr && preprocessor->name() == "png-optimizer") {
                selectedAutoOwned = createPreprocessor("png-optimizer", config);
            }
            if (selectedAutoOwned) {
                preprocessor = selectedAutoOwned.get();
            }
        } else {
            selectedByName = createPreprocessor(config.preprocessMode, config);
            preprocessor = selectedByName.get();
        }

        if (preprocessor != nullptr) {
            preprocessMetrics.type = preprocessor->name();
            preprocessMetrics.improvement = preprocessor->estimatedImprovement(fileType);
            if (preprocessor->canProcess(fileType)) {
                std::istringstream preprocessInput(rawContent);
                std::ostringstream preprocessOutput;
                const auto preprocessStart = std::chrono::steady_clock::now();
                const PreprocessResult preprocessResult =
                    preprocessor->preprocess(preprocessInput, preprocessOutput, fileType);
                const auto preprocessEnd = std::chrono::steady_clock::now();

                contentToCompress = preprocessOutput.str();
                preprocessMetrics.originalBytes =
                    preprocessResult.originalBytes > 0 ? preprocessResult.originalBytes : rawContent.size();
                preprocessMetrics.processedBytes =
                    preprocessResult.processedBytes > 0 ? preprocessResult.processedBytes : contentToCompress.size();
                preprocessMetrics.preprocessingTimeMs =
                    std::chrono::duration<double, std::milli>(preprocessEnd - preprocessStart).count();
                preprocessMetrics.applied = true;
            }
        }
    }

    std::istringstream input(contentToCompress);
    std::ostringstream compressed;
    CompressionOptions opts{};
    opts.level = level;
    opts.extreme = level >= engine->maxLevel();

    const auto encodeStart = std::chrono::steady_clock::now();
    CompressionResult encodeResult = engine->compress(input, compressed, opts);
    const auto encodeEnd = std::chrono::steady_clock::now();
    const auto encodeDuration = std::chrono::duration_cast<std::chrono::milliseconds>(encodeEnd - encodeStart);

    if (encodeDuration > config.maxTimePerFile) {
        throw std::runtime_error(
            "Compression exceeded max time for file: " + file.string() + " (" + engine->name() + ")");
    }

    std::chrono::milliseconds decodeDuration{0};
    if (config.runDecode) {
        std::istringstream compressedInput(compressed.str());
        std::ostringstream decompressed;

        const auto decodeStart = std::chrono::steady_clock::now();
        engine->decompress(compressedInput, decompressed);
        const auto decodeEnd = std::chrono::steady_clock::now();
        decodeDuration = std::chrono::duration_cast<std::chrono::milliseconds>(decodeEnd - decodeStart);

        if (decodeDuration > config.maxTimePerFile) {
            throw std::runtime_error(
                "Decompression exceeded max time for file: " + file.string() + " (" + engine->name() + ")");
        }
    }

    BenchmarkResult row{};
    row.algorithm = engine->name();
    row.level = level;
    row.file = file;
    row.fileType = fileType;
    row.originalBytes = config.usePreprocessing() ? rawContent.size() : encodeResult.originalBytes;
    row.compressedBytes = encodeResult.compressedBytes;
    row.encodeTime = encodeDuration;
    row.decodeTime = decodeDuration;
    row.peakMemoryBytes = config.trackMemory ? encodeResult.peakMemoryBytes : 0;
    row.preprocess = preprocessMetrics;
    return row;
}

std::vector<BenchmarkResult> BenchmarkRunner::run(
    const std::vector<std::filesystem::path>& files,
    const std::vector<std::string>& algorithms,
    const std::vector<int>& levels) {
    BenchmarkConfig config;
    config.files = files;
    config.algorithms = algorithms;
    config.levels = levels;
    return runWithConfig(config);
}

std::vector<BenchmarkResult> BenchmarkRunner::runGrid(
    const std::vector<std::filesystem::path>& files) {
    BenchmarkConfig config;
    config.files = files;
    config.allEngines = true;
    return runWithConfig(config);
}

std::vector<BenchmarkResult> BenchmarkRunner::runWithConfig(const BenchmarkConfig& config) {
    if (engines_.empty()) {
        throw std::runtime_error("No engines registered in BenchmarkRunner");
    }

    if (config.files.empty()) {
        return {};
    }

    std::vector<ICompressionEngine*> selectedEngines = selectEngines(engines_, config);

    if (selectedEngines.empty()) {
        throw std::runtime_error("No matching engines found for requested algorithms");
    }

    const std::unordered_map<std::string, std::vector<int>> levelsByAlgorithm = buildLevelMap(selectedEngines, config);

    const int iterations = std::max(1, config.iterations);
    const int warmups = std::max(0, config.warmupIterations);
    int unitsPerFile = 0;
    for (auto* engine : selectedEngines) {
        unitsPerFile += static_cast<int>(levelsByAlgorithm.at(engine->name()).size()) * (iterations + warmups);
    }
    const int totalWork = std::max(1, unitsPerFile * static_cast<int>(config.files.size()));
    int completedWork = 0;

    FileTypeDetector detector;
    std::vector<BenchmarkResult> results;

    auto lastProgressEmit = std::chrono::steady_clock::time_point{};
    auto emitProgress = [&](const std::filesystem::path& file,
                            const std::string& algorithm,
                            int level,
                            int currentIteration,
                            bool force) {
        if (!config.hasProgressCallback()) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (!force && lastProgressEmit != std::chrono::steady_clock::time_point{} &&
            (now - lastProgressEmit) < std::chrono::milliseconds(100)) {
            return;
        }

        ProgressInfo info{};
        info.currentFile = file;
        info.currentAlgorithm = algorithm;
        info.currentLevel = level;
        info.currentIteration = currentIteration;
        info.totalIterations = iterations + warmups;
        info.totalFiles = static_cast<int>(config.files.size());
        info.completedFiles = unitsPerFile > 0 ? completedWork / unitsPerFile : 0;
        info.progress = static_cast<double>(completedWork) / static_cast<double>(totalWork);
        config.onProgress(info);
        lastProgressEmit = now;
    };

    for (const auto& file : config.files) {
        if (config.hasCancellation() && config.shouldCancel()) {
            break;
        }

        const auto classification = detector.detect(file);
        for (auto* engine : selectedEngines) {
            const auto& levelSet = levelsByAlgorithm.at(engine->name());
            for (int level : levelSet) {
                emitProgress(file, engine->name(), level, 0, true);
                for (int i = 0; i < warmups; ++i) {
                    (void)runIteration(engine, file, level, config, classification.type);
                    ++completedWork;
                    emitProgress(file, engine->name(), level, i + 1, false);
                }

                for (int i = 0; i < iterations; ++i) {
                    results.push_back(runIteration(engine, file, level, config, classification.type));
                    ++completedWork;
                    emitProgress(file, engine->name(), level, warmups + i + 1, false);
                }
            }
        }
    }

    emitProgress({}, "", 0, 0, true);
    return results;
}

void BenchmarkRunner::processFile(
    const std::filesystem::path& file,
    const std::vector<ICompressionEngine*>& engines,
    const BenchmarkConfig& config,
    std::vector<BenchmarkResult>& results,
    std::mutex& resultsMutex,
    std::atomic<int>& completedCount,
    std::atomic<size_t>& completedBytes) const {
    FileTypeDetector detector;
    const auto classification = detector.detect(file);
    const auto levelsByAlgorithm = buildLevelMap(engines, config);
    const int iterations = std::max(1, config.iterations);
    const int warmups = std::max(0, config.warmupIterations);

    std::vector<BenchmarkResult> localResults;
    for (ICompressionEngine* engine : engines) {
        if (config.hasCancellation() && config.shouldCancel()) {
            break;
        }
        auto it = levelsByAlgorithm.find(engine->name());
        if (it == levelsByAlgorithm.end()) {
            continue;
        }
        const auto& levelSet = it->second;

        for (int level : levelSet) {
            if (config.hasCancellation() && config.shouldCancel()) {
                break;
            }
            for (int i = 0; i < warmups; ++i) {
                if (config.hasCancellation() && config.shouldCancel()) {
                    break;
                }
                (void)runIteration(engine, file, level, config, classification.type);
            }
            for (int i = 0; i < iterations; ++i) {
                if (config.hasCancellation() && config.shouldCancel()) {
                    break;
                }
                localResults.push_back(runIteration(engine, file, level, config, classification.type));
            }
        }
    }

    if (!localResults.empty()) {
        std::lock_guard<std::mutex> lock(resultsMutex);
        results.insert(results.end(), localResults.begin(), localResults.end());
    }

    std::error_code ec;
    const auto bytes = static_cast<size_t>(std::filesystem::file_size(file, ec));
    if (!ec) {
        completedBytes.fetch_add(bytes, std::memory_order_relaxed);
    }
    completedCount.fetch_add(1, std::memory_order_relaxed);
}

std::vector<BenchmarkResult> BenchmarkRunner::runParallel(const BenchmarkConfig& config) {
    if (engines_.empty()) {
        throw std::runtime_error("No engines registered in BenchmarkRunner");
    }

    if (config.files.empty()) {
        return {};
    }

    std::vector<ICompressionEngine*> selectedEngines = selectEngines(engines_, config);
    if (selectedEngines.empty()) {
        throw std::runtime_error("No matching engines found for requested algorithms");
    }

    // Peak memory reporting is process-wide on supported platforms, so it
    // cannot be attributed per file/operation under concurrent execution.
    // Disable memory tracking for parallel runs to avoid misleading values.
    BenchmarkConfig parallelConfig = config;
    parallelConfig.trackMemory = false;

    const int threadCount = std::max(1, config.getEffectiveThreadCount());
    ThreadPool pool(static_cast<size_t>(threadCount));

    std::vector<BenchmarkResult> allResults;
    allResults.reserve(config.files.size() * selectedEngines.size() * std::max(1, config.iterations));
    std::mutex resultsMutex;
    std::atomic<int> completedCount{0};
    std::atomic<size_t> completedBytes{0};
    std::atomic<bool> cancelled{false};
    std::mutex progressMutex;
    auto lastProgress = std::chrono::steady_clock::time_point{};

    auto maybeReportProgress = [&](const std::filesystem::path& file, bool force) {
        if (!config.hasProgressCallback()) {
            return;
        }

        std::lock_guard<std::mutex> lock(progressMutex);
        const auto now = std::chrono::steady_clock::now();
        if (!force && lastProgress != std::chrono::steady_clock::time_point{} &&
            (now - lastProgress) < std::chrono::milliseconds(50)) {
            return;
        }

        ProgressInfo info{};
        info.currentFile = file;
        info.completedFiles = completedCount.load(std::memory_order_relaxed);
        info.totalFiles = static_cast<int>(config.files.size());
        info.progress = static_cast<double>(info.completedFiles) / static_cast<double>(info.totalFiles);
        config.onProgress(info);
        lastProgress = now;
    };

    std::vector<std::future<void>> futures;
    futures.reserve(config.files.size());
    for (const auto& file : config.files) {
        futures.push_back(pool.submit([&, file]() {
            if (cancelled.load(std::memory_order_relaxed)) {
                return;
            }
            if (config.hasCancellation() && config.shouldCancel()) {
                cancelled.store(true, std::memory_order_relaxed);
                return;
            }

            maybeReportProgress(file, true);
            processFile(
                file,
                selectedEngines,
                parallelConfig,
                allResults,
                resultsMutex,
                completedCount,
                completedBytes);
            maybeReportProgress(file, false);
        }));
    }

    pool.waitAll();
    for (auto& future : futures) {
        future.get();
    }

    maybeReportProgress({}, true);
    return allResults;
}

std::unordered_map<std::string, BenchmarkStats> BenchmarkRunner::computeStats(
    const std::vector<BenchmarkResult>& results) const {
    std::unordered_map<std::string, std::vector<const BenchmarkResult*>> grouped;
    for (const auto& result : results) {
        const std::string key = result.algorithm + "/" + std::to_string(result.level);
        grouped[key].push_back(&result);
    }

    std::unordered_map<std::string, BenchmarkStats> stats;
    for (const auto& [key, values] : grouped) {
        if (values.empty()) {
            continue;
        }

        std::vector<double> ratios;
        std::vector<double> encode;
        std::vector<double> decode;
        std::vector<double> memory;
        ratios.reserve(values.size());
        encode.reserve(values.size());
        decode.reserve(values.size());
        memory.reserve(values.size());

        double ratioSum = 0.0;
        double encodeSum = 0.0;
        double decodeSum = 0.0;
        double memorySum = 0.0;
        for (const BenchmarkResult* value : values) {
            const double ratio = value->ratio();
            const double encodeMs = static_cast<double>(value->encodeTime.count());
            const double decodeMs = static_cast<double>(value->decodeTime.count());
            const double memoryBytes = static_cast<double>(value->peakMemoryBytes);

            ratios.push_back(ratio);
            encode.push_back(encodeMs);
            decode.push_back(decodeMs);
            memory.push_back(memoryBytes);

            ratioSum += ratio;
            encodeSum += encodeMs;
            decodeSum += decodeMs;
            memorySum += memoryBytes;
        }

        BenchmarkStats row{};
        row.iterations = static_cast<int>(values.size());
        row.meanRatio = ratioSum / static_cast<double>(values.size());
        row.meanEncodeTime = encodeSum / static_cast<double>(values.size());
        row.meanDecodeTime = decodeSum / static_cast<double>(values.size());
        row.meanPeakMemory = memorySum / static_cast<double>(values.size());
        row.stdDevRatio = std::sqrt(variance(ratios, row.meanRatio));
        row.stdDevEncodeTime = std::sqrt(variance(encode, row.meanEncodeTime));
        row.stdDevDecodeTime = std::sqrt(variance(decode, row.meanDecodeTime));
        stats[key] = row;
    }
    return stats;
}

} // namespace mosqueeze::bench
