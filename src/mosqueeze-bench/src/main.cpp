#include <mosqueeze/bench/BenchmarkRunner.hpp>
#include <mosqueeze/bench/CorpusManager.hpp>
#include <mosqueeze/bench/Formatters.hpp>
#include <mosqueeze/bench/ProgressReporter.hpp>
#include <mosqueeze/bench/ResultsStore.hpp>
#include <mosqueeze/engines/BrotliEngine.hpp>
#include <mosqueeze/engines/LzmaEngine.hpp>
#include <mosqueeze/engines/ZpaqEngine.hpp>
#include <mosqueeze/engines/ZstdEngine.hpp>
#include <mosqueeze/RawFormat.hpp>

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

std::vector<std::string> splitCsv(const std::string& csv) {
    std::vector<std::string> values;
    std::stringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (!token.empty()) {
            values.push_back(token);
        }
    }
    return values;
}

std::vector<int> splitCsvIntegers(const std::string& csv) {
    std::vector<int> values;
    for (const auto& token : splitCsv(csv)) {
        values.push_back(std::stoi(token));
    }
    return values;
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string normalizePreprocessorName(const std::string& mode) {
    const std::string lowered = toLower(mode);
    if (lowered == "json-canon" || lowered == "json-canonical") {
        return "json-canonical";
    }
    if (lowered == "xml-canon" || lowered == "xml-canonical") {
        return "xml-canonical";
    }
    if (lowered == "image-meta-stripper" || lowered == "image-meta-strip") {
        return "image-meta-strip";
    }
    if (lowered == "bayer-raw") {
        return "bayer-raw";
    }
    if (lowered == "png-optimizer") {
        return "png-optimizer";
    }
    if (lowered == "none") {
        return "none";
    }
    return lowered;
}

std::vector<std::string> parsePreprocessorModes(const std::string& csv) {
    std::vector<std::string> modes;
    for (const auto& token : splitCsv(csv)) {
        modes.push_back(normalizePreprocessorName(token));
    }
    std::sort(modes.begin(), modes.end());
    modes.erase(std::unique(modes.begin(), modes.end()), modes.end());
    return modes;
}

std::string wildcardToRegex(const std::string& pattern) {
    std::string regex = "^";
    for (const char c : pattern) {
        switch (c) {
            case '*':
                regex += ".*";
                break;
            case '?':
                regex += ".";
                break;
            case '.':
            case '+':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '^':
            case '$':
            case '|':
            case '\\':
                regex += '\\';
                regex += c;
                break;
            default:
                regex += c;
                break;
        }
    }
    regex += "$";
    return regex;
}

std::vector<std::filesystem::path> globFiles(const std::string& pattern) {
    std::vector<std::filesystem::path> files;
    const std::regex matcher(wildcardToRegex(pattern), std::regex::icase);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::current_path())) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (std::regex_match(entry.path().filename().string(), matcher)) {
            files.push_back(entry.path());
        }
    }
    return files;
}

void appendDirectoryFiles(const std::filesystem::path& directory, std::vector<std::filesystem::path>& files) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
}

void appendStdinFiles(std::vector<std::filesystem::path>& files) {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty()) {
            files.emplace_back(line);
        }
    }
}

void dedupeFiles(std::vector<std::filesystem::path>& files) {
    std::vector<std::filesystem::path> normalized;
    normalized.reserve(files.size());
    for (const auto& file : files) {
        std::error_code ec;
        const auto canonical = std::filesystem::weakly_canonical(file, ec);
        normalized.push_back(ec ? std::filesystem::absolute(file) : canonical);
    }
    std::sort(normalized.begin(), normalized.end());
    normalized.erase(std::unique(normalized.begin(), normalized.end()), normalized.end());
    files = std::move(normalized);
}

std::vector<uint8_t> readPrefix(const std::filesystem::path& file, size_t maxBytes) {
    std::vector<uint8_t> buffer(maxBytes, 0);
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return {};
    }
    in.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    buffer.resize(static_cast<size_t>(in.gcount()));
    return buffer;
}

std::string compressionToString(mosqueeze::RawCompression compression) {
    switch (compression) {
        case mosqueeze::RawCompression::Uncompressed:
            return "uncompressed";
        case mosqueeze::RawCompression::LosslessCompressed:
            return "lossless-compressed";
        case mosqueeze::RawCompression::LossyCompressed:
            return "lossy-compressed";
        case mosqueeze::RawCompression::Unknown:
        default:
            return "unknown";
    }
}

void printRawDetectionDryRun(const std::vector<std::filesystem::path>& files, bool forceBayer) {
    std::cout << "RAW format detection\n";
    for (const auto& file : files) {
        const auto prefix = readPrefix(file, 512);
        auto detected = mosqueeze::RawFormatDetector::detect(prefix);
        if (!detected.has_value()) {
            detected = mosqueeze::RawFormatDetector::detectByExtension(file.filename().string());
        }

        if (!detected.has_value()) {
            std::cout << "  " << file.string() << " -> unknown (bayer: "
                      << (forceBayer ? "apply (forced)" : "skip by default")
                      << ")\n";
            continue;
        }

        const bool shouldApply = mosqueeze::RawFormatDetector::shouldApplyBayerPreprocessor(detected->compression);
        std::string action = shouldApply ? "apply" : "skip";
        if (!shouldApply && forceBayer && detected->compression != mosqueeze::RawCompression::LossyCompressed) {
            action = "apply (forced)";
        }
        if (detected->compression == mosqueeze::RawCompression::LossyCompressed) {
            action = "reject";
        }

        std::cout << "  " << file.string()
                  << " -> " << detected->manufacturer << " " << detected->extension
                  << " (" << compressionToString(detected->compression) << ")"
                  << " | bayer: " << action << '\n';
    }
}

struct ComparisonRow {
    std::string key;
    double meanRatio = 0.0;
    double meanEncodeMs = 0.0;
    double meanDecodeMs = 0.0;
};

std::unordered_map<std::string, ComparisonRow> summarize(const std::vector<mosqueeze::bench::BenchmarkResult>& rows) {
    struct Agg {
        double ratio = 0.0;
        double encode = 0.0;
        double decode = 0.0;
        int count = 0;
    };
    std::unordered_map<std::string, Agg> agg;
    for (const auto& row : rows) {
        const std::string key = row.file.string() + "|" + row.algorithm + "|" + std::to_string(row.level);
        auto& target = agg[key];
        target.ratio += row.ratio();
        target.encode += static_cast<double>(row.encodeTime.count());
        target.decode += static_cast<double>(row.decodeTime.count());
        ++target.count;
    }

    std::unordered_map<std::string, ComparisonRow> out;
    for (const auto& [key, value] : agg) {
        ComparisonRow row{};
        row.key = key;
        row.meanRatio = value.ratio / value.count;
        row.meanEncodeMs = value.encode / value.count;
        row.meanDecodeMs = value.decode / value.count;
        out[key] = row;
    }
    return out;
}

std::vector<mosqueeze::bench::BenchmarkResult> loadComparisonRows(const std::filesystem::path& file) {
    std::vector<mosqueeze::bench::BenchmarkResult> rows;
    if (!std::filesystem::exists(file)) {
        throw std::runtime_error("Comparison file does not exist: " + file.string());
    }

    const auto ext = file.extension().string();
    if (ext == ".json") {
        std::ifstream in(file, std::ios::binary);
        nlohmann::json payload;
        in >> payload;
        if (!payload.is_array()) {
            throw std::runtime_error("Comparison JSON must be an array");
        }
        for (const auto& item : payload) {
            mosqueeze::bench::BenchmarkResult row{};
            row.algorithm = item.value("algorithm", "");
            row.level = item.value("level", 0);
            row.file = item.value("file", "");
            row.originalBytes = item.value("originalBytes", static_cast<size_t>(0));
            row.compressedBytes = item.value("compressedBytes", static_cast<size_t>(0));
            row.encodeTime = std::chrono::milliseconds(item.value("encodeMs", 0));
            row.decodeTime = std::chrono::milliseconds(item.value("decodeMs", 0));
            row.peakMemoryBytes = item.value("peakMemoryBytes", static_cast<size_t>(0));
            rows.push_back(std::move(row));
        }
    } else {
        std::ifstream in(file, std::ios::binary);
        std::string line;
        if (!std::getline(in, line)) {
            return rows;
        }
        while (std::getline(in, line)) {
            if (line.empty()) {
                continue;
            }
            std::stringstream ss(line);
            std::vector<std::string> cols;
            std::string col;
            while (std::getline(ss, col, ',')) {
                cols.push_back(col);
            }
            if (cols.size() < 9) {
                continue;
            }
            mosqueeze::bench::BenchmarkResult row{};
            row.algorithm = cols[0];
            row.level = std::stoi(cols[1]);
            if (!cols[2].empty() && cols[2].front() == '"' && cols[2].back() == '"') {
                row.file = cols[2].substr(1, cols[2].size() - 2);
            } else {
                row.file = cols[2];
            }
            row.originalBytes = static_cast<size_t>(std::stoll(cols[4]));
            row.compressedBytes = static_cast<size_t>(std::stoll(cols[5]));
            row.encodeTime = std::chrono::milliseconds(std::stoll(cols[6]));
            row.decodeTime = std::chrono::milliseconds(std::stoll(cols[7]));
            row.peakMemoryBytes = static_cast<size_t>(std::stoll(cols[8]));
            rows.push_back(std::move(row));
        }
    }
    return rows;
}

void printComparison(
    const std::vector<mosqueeze::bench::BenchmarkResult>& current,
    const std::vector<mosqueeze::bench::BenchmarkResult>& previous,
    bool diffOnly) {
    const auto currentAgg = summarize(current);
    const auto previousAgg = summarize(previous);
    std::cout << "\n=== Comparison ===\n";
    std::cout << "Key | Ratio Delta | Encode Delta(ms) | Decode Delta(ms)\n";
    std::cout << "-------------------------------------------------------\n";
    for (const auto& [key, curr] : currentAgg) {
        auto it = previousAgg.find(key);
        if (it == previousAgg.end()) {
            if (!diffOnly) {
                std::cout << key << " | new | new | new\n";
            }
            continue;
        }
        const double ratioDelta = curr.meanRatio - it->second.meanRatio;
        const double encodeDelta = curr.meanEncodeMs - it->second.meanEncodeMs;
        const double decodeDelta = curr.meanDecodeMs - it->second.meanDecodeMs;
        if (diffOnly && std::abs(ratioDelta) < 1e-9 && std::abs(encodeDelta) < 1e-9 && std::abs(decodeDelta) < 1e-9) {
            continue;
        }
        std::cout << key << " | "
                  << std::fixed << std::setprecision(4) << ratioDelta << " | "
                  << std::fixed << std::setprecision(2) << encodeDelta << " | "
                  << std::fixed << std::setprecision(2) << decodeDelta << '\n';
    }
}

void registerDefaultEngines(mosqueeze::bench::BenchmarkRunner& runner) {
    runner.registerEngine(std::make_unique<mosqueeze::ZstdEngine>());
    runner.registerEngine(std::make_unique<mosqueeze::LzmaEngine>());
    runner.registerEngine(std::make_unique<mosqueeze::BrotliEngine>());
    runner.registerEngine(std::make_unique<mosqueeze::ZpaqEngine>());
}

} // namespace

int main(int argc, char* argv[]) {
    CLI::App app{"MoSqueeze Benchmark Tool", "mosqueeze-bench"};

    std::vector<std::filesystem::path> files;
    std::filesystem::path directory;
    std::string globPattern;
    bool useStdin = false;
    std::filesystem::path corpusPath{"benchmarks/corpus"};

    std::string algorithmsOpt;
    std::string levelsOpt;
    bool allEngines = false;
    bool defaultOnly = false;

    int iterations = 1;
    int warmup = 0;
    bool trackMemory = true;
    bool noMemory = false;
    int maxTime = 3600;
    bool runDecode = true;
    bool noDecode = false;
    int threadCount = 0;
    bool sequential = false;
    std::string preprocessMode = "none";
    bool forceBayer = false;
    std::string pngEngine = "libpng";
    int pngLevel = 9;
    bool stripMetadata = false;
    bool noStripMetadata = false;
    bool fastFilters = false;
    bool extendedMatrix = false;
    std::string preprocessorsOpt = "none";
    bool resume = false;
    bool verifyRoundTrip = false;

    std::filesystem::path outputDir{"benchmarks/results"};
    std::filesystem::path exportFile;
    std::string format = "json";
    bool exportJsonFlag = false;
    bool exportCsvFlag = false;
    bool verbose = false;
    bool quiet = false;
    bool summary = false;
    bool noColor = false;

    std::filesystem::path compareFile;
    bool diffOnly = false;

    bool dryRun = false;
    bool listEngines = false;
    bool showVersion = false;
    bool skipConstraints = false;

    app.add_option("-f,--file", files, "Add single file to benchmark");
    app.add_option("-d,--directory", directory, "Add all files from directory (recursive)");
    app.add_option("-g,--glob", globPattern, "Add files matching glob pattern");
    app.add_option("-c,--corpus", corpusPath, "Use standard corpus path");
    app.add_flag("--stdin", useStdin, "Read file paths from stdin");

    app.add_option("-a,--algorithms", algorithmsOpt, "Comma-separated algorithms");
    app.add_option("-l,--levels", levelsOpt, "Comma-separated levels");
    app.add_flag("--all-engines", allEngines, "Test all registered engines");
    app.add_flag("--default-only", defaultOnly, "Use only default level for each engine");

    app.add_option("-i,--iterations", iterations, "Number of iterations")->check(CLI::PositiveNumber);
    app.add_option("-w,--warmup", warmup, "Warmup iterations")->check(CLI::NonNegativeNumber);
    app.add_flag("--track-memory", trackMemory, "Track peak memory usage");
    app.add_flag("--no-memory", noMemory, "Disable memory tracking");
    app.add_option("--max-time", maxTime, "Max time per compression in seconds")->check(CLI::PositiveNumber);
    app.add_flag("--decode", runDecode, "Include decompression benchmark");
    app.add_flag("--no-decode", noDecode, "Skip decompression benchmark");
    app.add_option("--threads", threadCount, "Number of worker threads (0 = auto-detect)")
        ->check(CLI::NonNegativeNumber);
    app.add_flag("--sequential", sequential, "Force sequential execution (disable parallelism)");
    app.add_option(
           "--preprocess",
           preprocessMode,
           "Preprocessor mode: auto, none, bayer-raw, image-meta-strip, png-optimizer, json-canonical, xml-canonical")
        ->check(CLI::IsMember(
            {"auto", "none", "bayer-raw", "image-meta-strip", "png-optimizer", "json-canonical", "xml-canonical"}))
        ->default_val("none");
    app.add_flag("--force-bayer", forceBayer, "Force Bayer preprocessing even when RAW appears compressed");
    app.add_option("--png-engine", pngEngine, "PNG engine: libpng, oxipng")
        ->check(CLI::IsMember({"libpng", "oxipng"}))
        ->default_val("libpng");
    app.add_option("--png-level", pngLevel, "PNG compression level (libpng: 1-9, oxipng: 0-6)")
        ->check(CLI::NonNegativeNumber)
        ->default_val(9);
    app.add_flag("--strip-metadata", stripMetadata, "Strip non-essential PNG metadata chunks");
    app.add_flag("--no-strip-metadata", noStripMetadata, "Keep PNG metadata chunks");
    app.add_flag("--fast-filters", fastFilters, "Use faster PNG filter search (less exhaustive)");
    app.add_flag("--extended", extendedMatrix, "Extended matrix mode: test each file with all applicable preprocessors");
    app.add_option(
           "--preprocessors",
           preprocessorsOpt,
           "Preprocessor matrix for --extended: all or comma list (none,json-canon,xml-canon,png-optimizer,image-meta-stripper,bayer-raw)")
        ->default_val("none");
    app.add_flag("--resume", resume, "Resume from existing output database and skip already tested combinations");
    app.add_flag("--verify-roundtrip", verifyRoundTrip, "Verify decompressed output byte-for-byte against input");

    app.add_option("-o,--output", outputDir, "Output directory");
    app.add_option("--export", exportFile, "Export results to file");
    app.add_option("--format", format, "Export format: json,csv,markdown,html")
        ->check(CLI::IsMember({"json", "csv", "markdown", "html"}));
    app.add_flag("--json", exportJsonFlag, "Export JSON to <output>/results.json");
    app.add_flag("--csv", exportCsvFlag, "Export CSV to <output>/results.csv");
    app.add_flag("-v,--verbose", verbose, "Verbose console output with progress");
    app.add_flag("-q,--quiet", quiet, "Quiet mode (errors only)");
    app.add_flag("--summary", summary, "Print summary table only");
    app.add_flag("--no-color", noColor, "Disable ANSI colors");

    app.add_option("--compare", compareFile, "Compare with previous results (JSON/CSV)");
    app.add_flag("--diff-only", diffOnly, "Show only files with different results");

    app.add_flag("--dry-run", dryRun, "Show configuration without running");
    app.add_flag("--skip-constraints", skipConstraints, "Skip algorithm size constraints (run all levels regardless of file size)");
    app.add_flag("--list-engines", listEngines, "List available engines and levels");
    app.add_flag("--version", showVersion, "Show version");

    app.get_formatter()->column_width(34);
    CLI11_PARSE(app, argc, argv);

    (void)noColor;
    trackMemory = trackMemory && !noMemory;
    runDecode = runDecode && !noDecode;
    if (verifyRoundTrip) {
        runDecode = true;
    }

    mosqueeze::bench::BenchmarkRunner runner;
    registerDefaultEngines(runner);

    if (showVersion) {
        std::cout << "mosqueeze-bench 0.2.0\n";
        return 0;
    }

    if (listEngines) {
        std::cout << "Available engines:\n";
        for (const auto& name : runner.availableAlgorithms()) {
            const auto levels = runner.availableLevels(name);
            std::cout << "  - " << name << " [";
            for (size_t i = 0; i < levels.size(); ++i) {
                if (i > 0) {
                    std::cout << ", ";
                }
                std::cout << levels[i];
            }
            std::cout << "]\n";
        }
        return 0;
    }

    if (!directory.empty()) {
        appendDirectoryFiles(directory, files);
    }
    if (!globPattern.empty()) {
        auto globbed = globFiles(globPattern);
        files.insert(files.end(), globbed.begin(), globbed.end());
    }
    if (useStdin) {
        appendStdinFiles(files);
    }
    if (files.empty()) {
        mosqueeze::bench::CorpusManager corpus(corpusPath);
        corpus.downloadStandardCorpus();
        for (const auto& file : corpus.listFiles()) {
            files.push_back(file.path);
        }
    }
    dedupeFiles(files);

    mosqueeze::bench::BenchmarkConfig config;
    config.files = files;
    config.useStdin = useStdin;
    config.corpusPath = corpusPath;
    config.algorithms = splitCsv(algorithmsOpt);
    config.levels = splitCsvIntegers(levelsOpt);
    config.allEngines = allEngines;
    config.defaultOnly = defaultOnly;
    config.iterations = iterations;
    config.warmupIterations = warmup;
    config.trackMemory = trackMemory;
    config.runDecode = runDecode;
    config.maxTimePerFile = std::chrono::seconds(maxTime);
    config.threadCount = threadCount;
    config.sequential = sequential;
    config.preprocessMode = preprocessMode;
    config.forceBayer = forceBayer;
    config.pngEngine = pngEngine;
    config.pngStripMetadata = !noStripMetadata || stripMetadata;
    config.pngAllFilters = !fastFilters;
    config.extendedMatrix = extendedMatrix;
    config.verifyRoundTrip = verifyRoundTrip;
    config.skipConstraints = skipConstraints;
    config.quiet = quiet;
    const auto requestedPreprocessors = parsePreprocessorModes(preprocessorsOpt);
    config.preprocessAll = preprocessorsOpt == "all" || preprocessorsOpt == "ALL";
    if (!config.preprocessAll) {
        config.preprocessModes = requestedPreprocessors;
    }
    if (pngEngine == "oxipng") {
        config.pngLevel = std::clamp(pngLevel, 0, 6);
    } else {
        config.pngLevel = std::clamp(pngLevel, 1, 9);
    }

    if (dryRun) {
        std::cout << "Configuration\n";
        std::cout << "  files: " << config.files.size() << '\n';
        std::cout << "  algorithms: " << (config.algorithms.empty() ? "all" : algorithmsOpt) << '\n';
        std::cout << "  levels: " << (config.levels.empty() ? "engine defaults" : levelsOpt) << '\n';
        std::cout << "  allEngines: " << (config.allEngines ? "true" : "false") << '\n';
        std::cout << "  defaultOnly: " << (config.defaultOnly ? "true" : "false") << '\n';
        std::cout << "  iterations: " << config.iterations << '\n';
        std::cout << "  warmupIterations: " << config.warmupIterations << '\n';
        std::cout << "  trackMemory: " << (config.trackMemory ? "true" : "false") << '\n';
        std::cout << "  runDecode: " << (config.runDecode ? "true" : "false") << '\n';
        std::cout << "  maxTimePerFile: " << config.maxTimePerFile.count() << "s\n";
        std::cout << "  threadCount: " << config.threadCount << '\n';
        std::cout << "  sequential: " << (config.sequential ? "true" : "false") << '\n';
        std::cout << "  preprocess: " << config.preprocessMode << '\n';
        std::cout << "  extendedMatrix: " << (config.extendedMatrix ? "true" : "false") << '\n';
        std::cout << "  preprocessors: " << (config.preprocessAll ? "all" : preprocessorsOpt) << '\n';
        std::cout << "  resume: " << (resume ? "true" : "false") << '\n';
        std::cout << "  verifyRoundTrip: " << (config.verifyRoundTrip ? "true" : "false") << '\n';
        std::cout << "  forceBayer: " << (config.forceBayer ? "true" : "false") << '\n';
        std::cout << "  pngEngine: " << config.pngEngine << '\n';
        std::cout << "  pngLevel: " << config.pngLevel << '\n';
        std::cout << "  stripMetadata: " << (config.pngStripMetadata ? "true" : "false") << '\n';
        std::cout << "  allFilters: " << (config.pngAllFilters ? "true" : "false") << '\n';
        if (config.preprocessMode == "bayer-raw" || config.preprocessMode == "auto") {
            printRawDetectionDryRun(config.files, config.forceBayer);
        }
        std::cout << "  effectiveThreads: " << config.getEffectiveThreadCount() << '\n';
        return 0;
    }

    std::unique_ptr<mosqueeze::bench::ProgressReporter> progressReporter;
    if (!quiet && !config.files.empty()) {
        progressReporter = std::make_unique<mosqueeze::bench::ProgressReporter>(
            config.files.size(),
            verbose,
            quiet);
        config.onProgress = [&progressReporter](const mosqueeze::bench::ProgressInfo& info) {
            if (progressReporter) {
                progressReporter->onProgress(info);
            }
        };
    }

    std::filesystem::create_directories(outputDir);
    mosqueeze::bench::ResultsStore store(outputDir / "results.sqlite3");
    if (resume) {
        config.skipExistingKeys = store.loadExistingKeys();
    } else {
        store.clear();
    }

    const auto startedAt = std::chrono::steady_clock::now();
    std::vector<mosqueeze::bench::BenchmarkResult> results;
    const bool useParallel = config.getEffectiveThreadCount() > 1 && config.files.size() > 1;
    if (useParallel) {
        if (verbose) {
            std::cout << "Running with " << config.getEffectiveThreadCount() << " threads...\n";
        }
        results = runner.runParallel(config);
    } else {
        results = runner.runWithConfig(config);
    }
    const auto stats = runner.computeStats(results);
    const auto finishedAt = std::chrono::steady_clock::now();
    progressReporter.reset();

    store.saveAll(results);

    if (!exportFile.empty()) {
        std::filesystem::create_directories(exportFile.parent_path());
        if (format == "json") {
            store.exportJson(exportFile);
        } else if (format == "csv") {
            store.exportCsv(exportFile);
        } else if (format == "markdown") {
            std::ofstream out(exportFile, std::ios::binary);
            out << mosqueeze::bench::Formatters::exportMarkdown(results, &stats);
        } else if (format == "html") {
            std::ofstream out(exportFile, std::ios::binary);
            out << mosqueeze::bench::Formatters::exportHtml(results, &stats);
        }
    }

    if (exportFile.empty()) {
        if (exportJsonFlag || exportCsvFlag) {
            if (exportJsonFlag) {
                store.exportJson(outputDir / "results.json");
            }
            if (exportCsvFlag) {
                store.exportCsv(outputDir / "results.csv");
            }
        } else {
            store.exportJson(outputDir / "results.json");
            store.exportCsv(outputDir / "results.csv");
        }
    } else if (exportJsonFlag || exportCsvFlag) {
        if (exportJsonFlag) {
            store.exportJson(outputDir / "results.json");
        }
        if (exportCsvFlag) {
            store.exportCsv(outputDir / "results.csv");
        }
    }

    if (!quiet) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(finishedAt - startedAt).count();
        std::cout << "Completed " << results.size() << " benchmark samples in " << elapsed << " ms\n";
        if (summary || !verbose) {
            std::cout << mosqueeze::bench::Formatters::formatSummaryTable(results, &stats);
        } else {
            for (const auto& row : results) {
                const auto key = row.algorithm + "/" + std::to_string(row.level);
                auto it = stats.find(key);
                std::cout << mosqueeze::bench::Formatters::formatVerbose(
                    row, it != stats.end() ? &it->second : nullptr) << '\n';
            }
        }
    }

    if (!compareFile.empty()) {
        auto previous = loadComparisonRows(compareFile);
        printComparison(results, previous, diffOnly);
    }

    return 0;
}
