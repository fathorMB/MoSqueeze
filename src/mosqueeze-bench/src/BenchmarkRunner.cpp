#include <mosqueeze/bench/BenchmarkRunner.hpp>

#include <mosqueeze/FileTypeDetector.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace mosqueeze::bench {

BenchmarkRunner::BenchmarkRunner() = default;

void BenchmarkRunner::registerEngine(std::unique_ptr<ICompressionEngine> engine) {
    if (!engine) {
        throw std::runtime_error("Cannot register null compression engine");
    }
    engines_.push_back(std::move(engine));
}

ICompressionEngine* BenchmarkRunner::findEngine(const std::string& name) const {
    auto it = std::find_if(engines_.begin(), engines_.end(), [&](const auto& engine) {
        return engine->name() == name;
    });

    return it == engines_.end() ? nullptr : it->get();
}

std::vector<BenchmarkResult> BenchmarkRunner::run(
    const std::vector<std::filesystem::path>& files,
    const std::vector<std::string>& algorithms,
    const std::vector<int>& levels) {
    if (engines_.empty()) {
        throw std::runtime_error("No engines registered in BenchmarkRunner");
    }

    std::unordered_set<std::string> selectedAlgorithms(algorithms.begin(), algorithms.end());

    std::vector<ICompressionEngine*> selectedEngines;
    for (const auto& engine : engines_) {
        if (selectedAlgorithms.empty() || selectedAlgorithms.count(engine->name()) > 0) {
            selectedEngines.push_back(engine.get());
        }
    }

    if (selectedEngines.empty()) {
        throw std::runtime_error("No matching engines found for requested algorithms");
    }

    FileTypeDetector detector;
    std::vector<BenchmarkResult> results;

    for (const auto& file : files) {
        const auto classification = detector.detect(file);

        for (ICompressionEngine* engine : selectedEngines) {
            std::vector<int> levelSet;
            if (levels.empty()) {
                levelSet = engine->supportedLevels();
            } else {
                const auto supported = engine->supportedLevels();
                for (int level : levels) {
                    if (std::find(supported.begin(), supported.end(), level) != supported.end()) {
                        levelSet.push_back(level);
                    }
                }
                if (levelSet.empty()) {
                    levelSet.push_back(engine->defaultLevel());
                }
            }

            for (int level : levelSet) {
                std::ifstream in(file, std::ios::binary);
                if (!in) {
                    throw std::runtime_error("Failed to open file for benchmark: " + file.string());
                }

                std::ostringstream compressed;
                CompressionOptions opts{};
                opts.level = level;
                opts.extreme = level >= engine->maxLevel();

                const auto encodeStart = std::chrono::steady_clock::now();
                CompressionResult encodeResult = engine->compress(in, compressed, opts);
                const auto encodeEnd = std::chrono::steady_clock::now();

                std::istringstream compressedInput(compressed.str());
                std::ostringstream decompressed;

                const auto decodeStart = std::chrono::steady_clock::now();
                engine->decompress(compressedInput, decompressed);
                const auto decodeEnd = std::chrono::steady_clock::now();

                BenchmarkResult row{};
                row.algorithm = engine->name();
                row.level = level;
                row.file = file;
                row.fileType = classification.type;
                row.originalBytes = encodeResult.originalBytes;
                row.compressedBytes = encodeResult.compressedBytes;
                row.encodeTime = std::chrono::duration_cast<std::chrono::milliseconds>(encodeEnd - encodeStart);
                row.decodeTime = std::chrono::duration_cast<std::chrono::milliseconds>(decodeEnd - decodeStart);
                row.peakMemoryBytes = encodeResult.peakMemoryBytes;

                results.push_back(std::move(row));
            }
        }
    }

    return results;
}

std::vector<BenchmarkResult> BenchmarkRunner::runGrid(
    const std::vector<std::filesystem::path>& files) {
    return run(files, {}, {});
}

} // namespace mosqueeze::bench
