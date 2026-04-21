# Worker Spec: Enhanced Benchmark Tool (Issue #23)

## Overview

Migliorare `mosqueeze-bench` con CLI avanzato per benchmark intensivi su file arbitrari.

## Stato Attuale

Il tool esistente (`src/mosqueeze-bench/`) ha:
- ✅ `BenchmarkRunner` con registrazione engine
- ✅ `run()` con opzioni algoritmi/levels
- ✅ `runGrid()` per tutti gli algoritmi
- ✅ `ResultsStore` SQLite con export JSON/CSV
- ✅ `CorpusManager` per corpus standard
- ✅ `BenchmarkResult` struct con ratio/encode/decode/memory

**File esistenti:**
- `src/mosqueeze-bench/src/main.cpp` — CLI entry point
- `src/mosqueeze-bench/src/BenchmarkRunner.cpp` — Runner implementation
- `src/mosqueeze-bench/src/CorpusManager.cpp` — Corpus handling
- `src/mosqueeze-bench/src/ResultsStore.cpp` — SQLite storage
- `include/mosqueeze/bench/BenchmarkResult.hpp` — Result struct

**Mancano:**
- ❌ Specifica file arbitrari da CLI (`--file`, `--directory`, `--glob`)
- ❌ Iterazioni multiple + warmup
- ❌ Progress callback con output live
- ❌ Output formattato tabellare
- ❌ Export Markdown/HTML
- ❌ Statistiche aggregate (media, deviazione standard)
- ❌ Time limits per file
- ❌ Comparison con run precedenti

---

## Parte 1: CLI Options Enhancement

### Nuovi Parametri

```
mosqueeze-bench [OPTIONS]

Input:
  -f, --file PATH          Add single file to benchmark
  -d, --directory PATH     Add all files from directory (recursive)
  -g, --glob PATTERN       Add files matching glob pattern
  -c, --corpus PATH        Use standard corpus (default: benchmarks/corpus)
      --stdin              Read file paths from stdin (one per line)

Algorithm:
  -a, --algorithms LIST    Comma-separated algorithms (zstd,lzma,brotli,zpaq)
  -l, --levels LIST        Comma-separated levels (1,5,9,22)
      --all-engines        Test all registered engines with all levels
      --default-only       Use only default level for each engine

Benchmark:
  -i, --iterations N       Number of iterations (default: 1)
  -w, --warmup N           Warmup iterations excluded from stats (default: 0)
      --track-memory       Track peak memory usage (default: on)
      --no-memory          Disable memory tracking
      --max-time SECONDS   Max time per compression (default: 3600)
      --decode             Include decompression benchmark (default: on)
      --no-decode          Skip decompression benchmark

Output:
  -o, --output DIR         Output directory (default: benchmarks/results)
      --export FILE        Export results to file
      --format FORMAT      Export format: json, csv, markdown, html
  -v, --verbose            Verbose console output with progress
  -q, --quiet              Quiet mode (errors only)
      --summary            Print summary table only
      --no-color           Disable ANSI colors

Comparison:
      --compare FILE       Compare with previous results (JSON/CSV)
      --diff-only          Show only files with different results

Misc:
      --dry-run            Show configuration without running
      --list-engines       List available engines and their levels
      --version            Show version
  -h, --help               Show this help
```

### Esempi d'Uso

```bash
# Benchmark singolo file con tutti gli algoritmi
mosqueeze-bench --file my_video.mkv --all-engines --verbose

# Benchmark directory con iterazioni
mosqueeze-bench --directory ./data/ --iterations 5 --warmup 2

# Solo ZPAQ su file video
mosqueeze-bench --glob "*.mkv" --algorithms zpaq --levels 5

# Confronto con run precedente
mosqueeze-bench --file test.db --compare benchmarks/results/old.json

# Leggi file da stdin
find . -name "*.json" | mosqueeze-bench --stdin --iterations 3
```

---

## Parte 2: BenchmarkConfig Struct

### Nuovo Header

`include/mosqueeze/bench/BenchmarkConfig.hpp`:

```cpp
#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace mosqueeze::bench {

struct ProgressInfo {
    std::filesystem::path currentFile;
    std::string currentAlgorithm;
    int currentLevel = 0;
    int currentIteration = 0;
    int totalIterations = 0;
    int totalFiles = 0;
    int completedFiles = 0;
    double progress = 0.0;  // 0.0 - 1.0
};

struct BenchmarkStats {
    double meanRatio = 0.0;
    double stdDevRatio = 0.0;
    double meanEncodeTime = 0.0;
    double stdDevEncodeTime = 0.0;
    double meanDecodeTime = 0.0;
    double stdDevDecodeTime = 0.0;
    double meanPeakMemory = 0.0;
    int iterations = 0;
};

struct BenchmarkConfig {
    // Input
    std::vector<std::filesystem::path> files;
    bool useStdin = false;
    std::filesystem::path corpusPath{"benchmarks/corpus"};
    
    // Algorithms
    std::vector<std::string> algorithms;
    std::vector<int> levels;
    bool allEngines = false;
    bool defaultOnly = false;
    
    // Benchmark
    int iterations = 1;
    int warmupIterations = 0;
    bool trackMemory = true;
    bool runDecode = true;
    std::chrono::seconds maxTimePerFile{3600};
    
    // Callbacks
    std::function<void(const ProgressInfo&)> onProgress;
    std::function<bool()> shouldCancel;  // Return true to cancel
    
    // Query methods
    bool hasProgressCallback() const { return static_cast<bool>(onProgress); }
    bool hasCancellation() const { return static_cast<bool>(shouldCancel); }
};

} // namespace mosqueeze::bench
```

---

## Parte 3: BenchmarkRunner Enhancement

### Aggiornamento Header

`include/mosqueeze/bench/BenchmarkRunner.hpp`:

```cpp
#pragma once

#include <mosqueeze/ICompressionEngine.hpp>
#include <mosqueeze/bench/BenchmarkConfig.hpp>
#include <mosqueeze/bench/BenchmarkResult.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mosqueeze::bench {

class BenchmarkRunner {
public:
    BenchmarkRunner();
    ~BenchmarkRunner();

    // Engine management
    void registerEngine(std::unique_ptr<ICompressionEngine> engine);
    std::vector<std::string> availableAlgorithms() const;
    std::vector<int> availableLevels(const std::string& algorithm) const;
    
    // Legacy API (backward compatible)
    std::vector<BenchmarkResult> run(
        const std::vector<std::filesystem::path>& files,
        const std::vector<std::string>& algorithms = {},
        const std::vector<int>& levels = {});

    std::vector<BenchmarkResult> runGrid(
        const std::vector<std::filesystem::path>& files);
    
    // New API with config
    std::vector<BenchmarkResult> runWithConfig(const BenchmarkConfig& config);
    
    // Statistics
    std::unordered_map<std::string, BenchmarkStats> computeStats(
        const std::vector<BenchmarkResult>& results) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    ICompressionEngine* findEngine(const std::string& name) const;
    void runIteration(
        ICompressionEngine* engine,
        const std::filesystem::path& file,
        int level,
        const CompressionOptions& opts,
        BenchmarkResult& result);
};

} // namespace mosqueeze::bench
```

### Implementation Key Methods

```cpp
std::vector<BenchmarkResult> BenchmarkRunner::runWithConfig(const BenchmarkConfig& config) {
    if (engines_.empty()) {
        throw std::runtime_error("No engines registered");
    }
    
    // Select engines
    std::vector<ICompressionEngine*> selectedEngines;
    if (config.allEngines || config.algorithms.empty()) {
        for (const auto& e : engines_) {
            selectedEngines.push_back(e.get());
        }
    } else {
        for (const auto& name : config.algorithms) {
            auto* engine = findEngine(name);
            if (engine) selectedEngines.push_back(engine);
        }
    }
    
    FileTypeDetector detector;
    std::vector<BenchmarkResult> allResults;
    
    int totalWork = config.files.size() * selectedEngines.size() 
                  * config.iterations * levels.size();
    int completedWork = 0;
    
    for (const auto& file : config.files) {
        // Check cancellation
        if (config.hasCancellation() && config.shouldCancel()) {
            break;
        }
        
        auto classification = detector.detect(file);
        
        for (ICompressionEngine* engine : selectedEngines) {
            std::vector<int> levelsToTest;
            if (config.defaultOnly) {
                levelsToTest.push_back(engine->defaultLevel());
            } else if (config.levels.empty()) {
                levelsToTest = engine->supportedLevels();
            } else {
                // Filter levels to supported ones
                auto supported = engine->supportedLevels();
                for (int l : config.levels) {
                    if (std::find(supported.begin(), supported.end(), l) != supported.end()) {
                        levelsToTest.push_back(l);
                    }
                }
            }
            
            for (int level : levelsToTest) {
                // Warmup iterations
                for (int w = 0; w < config.warmupIterations; ++w) {
                    BenchmarkResult dummy;
                    runIteration(engine, file, level, CompressionOptions{}, dummy);
                    
                    if (config.hasProgressCallback()) {
                        ProgressInfo info{};
                        info.currentFile = file;
                        info.currentAlgorithm = engine->name();
                        info.currentLevel = level;
                        info.currentIteration = w;
                        info.totalIterations = config.warmupIterations + config.iterations;
                        info.progress = static_cast<double>(completedWork) / totalWork;
                        config.onProgress(info);
                    }
                    ++completedWork;
                }
                
                // Measured iterations
                for (int iter = 0; iter < config.iterations; ++iter) {
                    BenchmarkResult result;
                    runIteration(engine, file, level, CompressionOptions{}, result);
                    result.fileType = classification.type;
                    allResults.push_back(result);
                    
                    if (config.hasProgressCallback()) {
                        ProgressInfo info{};
                        info.currentFile = file;
                        info.currentAlgorithm = engine->name();
                        info.currentLevel = level;
                        info.currentIteration = config.warmupIterations + iter;
                        info.totalIterations = config.warmupIterations + config.iterations;
                        info.completedFiles = completedWork / (levelsToTest.size() * selectedEngines.size());
                        info.totalFiles = config.files.size();
                        info.progress = static_cast<double>(completedWork) / totalWork;
                        config.onProgress(info);
                    }
                    ++completedWork;
                }
            }
        }
    }
    
    return allResults;
}

void BenchmarkRunner::runIteration(
    ICompressionEngine* engine,
    const std::filesystem::path& file,
    int level,
    const CompressionOptions& opts,
    BenchmarkResult& result) {
    
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open file: " + file.string());
    }
    
    CompressionOptions optsCopy = opts;
    optsCopy.level = level;
    optsCopy.extreme = (level == engine->maxLevel());
    
    std::ostringstream compressed;
    auto encodeStart = std::chrono::steady_clock::now();
    CompressionResult encodeResult = engine->compress(in, compressed, optsCopy);
    auto encodeEnd = std::chrono::steady_clock::now();
    
    result.algorithm = engine->name();
    result.level = level;
    result.file = file;
    result.originalBytes = encodeResult.originalBytes;
    result.compressedBytes = encodeResult.compressedBytes;
    result.encodeTime = std::chrono::duration_cast<std::chrono::milliseconds>(encodeEnd - encodeStart);
    result.peakMemoryBytes = encodeResult.peakMemoryBytes;
    
    // Decode if requested
    // ... (decompression timing)
}

std::unordered_map<std::string, BenchmarkStats> BenchmarkRunner::computeStats(
    const std::vector<BenchmarkResult>& results) const {
    
    // Group by algorithm/level
    std::unordered_map<std::string, std::vector<BenchmarkResult>> grouped;
    for (const auto& r : results) {
        std::string key = r.algorithm + "/" + std::to_string(r.level);
        grouped[key].push_back(r);
    }
    
    std::unordered_map<std::string, BenchmarkStats> stats;
    for (const auto& [key, values] : grouped) {
        BenchmarkStats s;
        s.iterations = values.size();
        
        // Compute mean
        double sumRatio = 0, sumEncode = 0, sumDecode = 0;
        for (const auto& v : values) {
            sumRatio += v.ratio();
            sumEncode += v.encodeTime.count();
            sumDecode += v.decodeTime.count();
        }
        s.meanRatio = sumRatio / values.size();
        s.meanEncodeTime = sumEncode / values.size();
        s.meanDecodeTime = sumDecode / values.size();
        
        // Compute std dev
        double varRatio = 0, varEncode = 0;
        for (const auto& v : values) {
            varRatio += (v.ratio() - s.meanRatio) * (v.ratio() - s.meanRatio);
            varEncode += (v.encodeTime.count() - s.meanEncodeTime) 
                       * (v.encodeTime.count() - s.meanEncodeTime);
        }
        s.stdDevRatio = std::sqrt(varRatio / values.size());
        s.stdDevEncodeTime = std::sqrt(varEncode / values.size());
        
        stats[key] = s;
    }
    
    return stats;
}
```

---

## Parte 4: Output Formatters

### Console Output (verboso)

```cpp
// In src/mosqueeze-bench/src/ConsoleFormatter.cpp

std::string formatVerbose(const BenchmarkResult& result, const BenchmarkStats* stats = nullptr) {
    std::ostringstream out;
    
    // File header
    out << "\n═══════════════════════════════════════════════════════════\n";
    out << result.file.filename().string() << " (" << formatBytes(result.originalBytes) << ")";
    out << " - " << fileTypeToString(result.fileType) << "\n";
    out << "═══════════════════════════════════════════════════════════\n\n";
    
    // Algorithm results
    out << "Algorithm    Level    Ratio    Encode    Decode    Memory\n";
    out << "─────────────────────────────────────────────────────────────\n";
    
    out << std::left << std::setw(12) << result.algorithm;
    out << std::setw(8) << result.level;
    out << std::setw(9) << std::fixed << std::setprecision(2) << result.ratio() << "x";
    out << std::setw(9) << formatDuration(result.encodeTime);
    out << std::setw(9) << formatDuration(result.decodeTime);
    out << std::setw(10) << formatBytes(result.peakMemoryBytes);
    out << "\n";
    
    if (stats) {
        out << "\n  Stats (n=" << stats->iterations << "):\n";
        out << "    Ratio: " << stats->meanRatio << "x ± " << stats->stdDevRatio << "\n";
        out << "    Encode: " << stats->meanEncodeTime << "ms ± " << stats->stdDevEncodeTime << "\n";
    }
    
    return out.str();
}
```

### Markdown Export

```cpp
std::string exportMarkdown(const std::vector<BenchmarkResult>& results) {
    std::ostringstream out;
    
    out << "# Benchmark Results\n\n";
    out << "## Summary\n\n";
    
    // Group by file
    std::map<std::filesystem::path, std::vector<BenchmarkResult>> byFile;
    for (const auto& r : results) {
        byFile[r.file].push_back(r);
    }
    
    for (const auto& [file, fileResults] : byFile) {
        out << "### " << file.filename().string() << "\n\n";
        out << "| Algorithm | Level | Ratio | Encode | Decode | Memory |\n";
        out << "|-----------|-------|-------|--------|--------|--------|\n";
        
        // Sort by ratio descending
        auto sorted = fileResults;
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
            return a.ratio() > b.ratio();
        });
        
        for (const auto& r : sorted) {
            out << "| " << r.algorithm << " | " << r.level << " | ";
            out << std::fixed << std::setprecision(2) << r.ratio() << "x | ";
            out << r.encodeTime.count() << "ms | ";
            out << r.decodeTime.count() << "ms | ";
            out << formatBytes(r.peakMemoryBytes) << " |\n";
        }
        
        out << "\n**Best:** " << sorted.front().algorithm << "/" << sorted.front().level << "\n\n";
    }
    
    return out.str();
}
```

### HTML Export with Charts

```cpp
std::string exportHtml(const std::vector<BenchmarkResult>& results) {
    std::ostringstream out;
    
    out << R"(<!DOCTYPE html>
<html>
<head>
    <title>MoSqueeze Benchmark Results</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        body { font-family: system-ui, sans-serif; margin: 40px; }
        table { border-collapse: collapse; width: 100%; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background: #444; color: white; }
        tr:nth-child(even) { background: #f8f8f8; }
        .best { background: #d4edda; }
    </style>
</head>
<body>
    <h1>MoSqueeze Benchmark Results</h1>
)";

    // Charts and tables...
    
    out << "</body></html>";
    return out.str();
}
```

---

## Parte 5: CLI Implementation

### main.cpp Enhancement

```cpp
#include <mosqueeze/bench/BenchmarkRunner.hpp>
#include <mosqueeze/bench/BenchmarkConfig.hpp>
#include <mosqueeze/bench/CorpusManager.hpp>
#include <mosqueeze/bench/ResultsStore.hpp>
#include <mosqueeze/bench/Formatters.hpp>

#include <mosqueeze/engines/ZstdEngine.hpp>
#include <mosqueeze/engines/LzmaEngine.hpp>
#include <mosqueeze/engines/BrotliEngine.hpp>
#include <mosqueeze/engines/ZpaqEngine.hpp>

#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    CLI::App app{"MoSqueeze Benchmark Tool", "mosqueeze-bench"};
    
    // --- Input options ---
    std::vector<std::filesystem::path> files;
    std::filesystem::path directory;
    std::string globPattern;
    bool useStdin = false;
    
    auto* fileOpt = app.add_option("-f,--file", files, "Files to benchmark");
    app.add_option("-d,--directory", directory, "Directory to benchmark (recursive)");
    app.add_option("-g,--glob", globPattern, "Glob pattern for files");
    app.add_flag("--stdin", useStdin, "Read file paths from stdin");
    
    // --- Algorithm options ---
    std::string algorithmsOpt;
    std::string levelsOpt;
    bool allEngines = false;
    bool defaultOnly = false;
    
    app.add_option("-a,--algorithms", algorithmsOpt, "Comma-separated algorithms");
    app.add_option("-l,--levels", levelsOpt, "Comma-separated levels");
    app.add_flag("--all-engines", allEngines, "Test all registered engines");
    app.add_flag("--default-only", defaultOnly, "Use default level only");
    
    // --- Benchmark options ---
    int iterations = 1;
    int warmup = 0;
    bool trackMemory = true;
    int maxTime = 3600;
    
    app.add_option("-i,--iterations", iterations, "Number of iterations", true);
    app.add_option("-w,--warmup", warmup, "Warmup iterations", true);
    app.add_flag("--track-memory", trackMemory, "Track memory (default: on)");
    app.add_option("--max-time", maxTime, "Max time per file (seconds)", true);
    
    // --- Output options ---
    std::filesystem::path outputDir{"benchmarks/results"};
    std::filesystem::path exportFile;
    std::string format = "json";
    bool verbose = false;
    bool quiet = false;
    bool summary = false;
    
    app.add_option("-o,--output", outputDir, "Output directory", true);
    app.add_option("--export", exportFile, "Export results to file");
    app.add_option("--format", format, "Export format", true)->check([](const std::string& s) {
        return (s == "json" || s == "csv" || s == "markdown" || s == "html") 
            ? "" : "Format must be json, csv, markdown, or html";
    });
    app.add_flag("-v,--verbose", verbose, "Verbose output");
    app.add_flag("-q,--quiet", quiet, "Quiet mode");
    app.add_flag("--summary", summary, "Summary only");
    
    // --- Misc options ---
    bool dryRun = false;
    bool listEngines = false;
    
    app.add_flag("--dry-run", dryRun, "Show configuration without running");
    app.add_flag("--list-engines", listEngines, "List available engines");
    
    CLI11_PARSE(app, argc, argv);
    
    // --- List engines ---
    if (listEngines) {
        std::cout << "Available engines:\n";
        // ... register engines and list
        return 0;
    }
    
    // --- Build config ---
    BenchmarkConfig config;
    
    // Parse files
    if (!directory.empty()) {
        for (auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                config.files.push_back(entry.path());
            }
        }
    }
    
    if (!globPattern.empty()) {
        // Use std::filesystem with glob
        // ...
    }
    
    config.files = files;
    config.allEngines = allEngines;
    config.defaultOnly = defaultOnly;
    config.iterations = iterations;
    config.warmupIterations = warmup;
    config.trackMemory = trackMemory;
    config.maxTimePerFile = std::chrono::seconds(maxTime);
    
    // Parse algorithms
    if (!algorithmsOpt.empty()) {
        std::stringstream ss(algorithmsOpt);
        std::string token;
        while (std::getline(ss, token, ',')) {
            config.algorithms.push_back(token);
        }
    }
    
    // Parse levels
    if (!levelsOpt.empty()) {
        std::stringstream ss(levelsOpt);
        std::string token;
        while (std::getline(ss, token, ',')) {
            config.levels.push_back(std::stoi(token));
        }
    }
    
    // --- Dry run ---
    if (dryRun) {
        std::cout << "Configuration:\n";
        std::cout << "  Files: " << config.files.size() << "\n";
        std::cout << "  Algorithms: " << (config.algorithms.empty() ? "(all)" : "") << "\n";
        std::cout << "  Iterations: " << config.iterations << "\n";
        std::cout << "  Warmup: " << config.warmupIterations << "\n";
        return 0;
    }
    
    // --- Progress callback ---
    if (verbose) {
        config.onProgress = [](const ProgressInfo& info) {
            std::cout << "\r[" << info.currentAlgorithm << "/" << info.currentLevel 
                      << "] " << info.currentFile.filename().string()
                      << " (" << std::fixed << std::setprecision(1) << (info.progress * 100) << "%)"
                      << std::flush;
        };
    }
    
    // --- Run benchmark ---
    BenchmarkRunner runner;
    runner.registerEngine(std::make_unique<ZstdEngine>());
    runner.registerEngine(std::make_unique<LzmaEngine>());
    runner.registerEngine(std::make_unique<BrotliEngine>());
    runner.registerEngine(std::make_unique<ZpaqEngine>());
    
    auto results = runner.runWithConfig(config);
    auto stats = runner.computeStats(results);
    
    // --- Output ---
    std::filesystem::create_directories(outputDir);
    ResultsStore store((outputDir / "results.sqlite3"));
    store.clear();
    store.saveAll(results);
    
    if (!exportFile.empty()) {
        if (format == "json") {
            store.exportJson(exportFile);
        } else if (format == "csv") {
            store.exportCsv(exportFile);
        } else if (format == "markdown") {
            std::ofstream out(exportFile);
            out << Formatters::exportMarkdown(results);
        } else if (format == "html") {
            std::ofstream out(exportFile);
            out << Formatters::exportHtml(results);
        }
    }
    
    // --- Print summary ---
    if (!quiet) {
        std::cout << "\n\n=== Best Results ===\n";
        // Sort and print top results
    }
    
    return 0;
}
```

---

## Parte 6: Test

### tests/unit/BenchmarkConfig_test.cpp

```cpp
#include <mosqueeze/bench/BenchmarkConfig.hpp>
#include <mosqueeze/bench/BenchmarkRunner.hpp>

#include <cassert>
#include <iostream>

int main() {
    mosqueeze::bench::BenchmarkConfig config;
    
    // Test defaults
    assert(config.iterations == 1);
    assert(config.warmupIterations == 0);
    assert(config.trackMemory == true);
    assert(config.maxTimePerFile.count() == 3600);
    
    // Test file addition
    config.files.push_back("/tmp/test.bin");
    assert(config.files.size() == 1);
    
    // Test progress callback
    bool progressCalled = false;
    config.onProgress = [&](const mosqueeze::bench::ProgressInfo& info) {
        progressCalled = true;
        assert(!info.currentFile.empty());
    };
    
    config.onProgress({});  // Test callback
    assert(progressCalled);
    
    std::cout << "[PASS] BenchmarkConfig tests\n";
    return 0;
}
```

### tests/integration/BenchmarkTool_test.cpp

```cpp
// Integration test with real files
// Requires test fixtures in tests/fixtures/
```

---

## Definizione di Fatto

- [ ] `BenchmarkConfig.hpp` con struct completo
- [ ] `BenchmarkRunner::runWithConfig()` implementata
- [ ] Progress callback funzionante
- [ ] Warmup iterations supportate
- [ ] CLI con `--file`, `--directory`, `--glob`, `--stdin`
- [ ] CLI con `--algorithms`, `--levels`, `--all-engines`, `--default-only`
- [ ] CLI con `--iterations`, `--warmup`
- [ ] Console output verboso
- [ ] Export Markdown
- [ ] Export HTML con charts (Chart.js o Plotly)
- [ ] `computeStats()` con media e std dev
- [ ] `--list-engines` funzionante
- [ ] `--dry-run` funzionante
- [ ] Test unitari per BenchmarkConfig
- [ ] CI passa su Linux, macOS, Windows

---

## File da Creare/Modificare

### Nuovi File

1. `src/libmosqueeze/include/mosqueeze/bench/BenchmarkConfig.hpp`
2. `src/mosqueeze-bench/src/Formatters.cpp`
3. `src/mosqueeze-bench/include/mosqueeze/bench/Formatters.hpp`
4. `tests/unit/BenchmarkConfig_test.cpp`

### File da Modificare

5. `src/mosqueeze-bench/include/mosqueeze/bench/BenchmarkRunner.hpp` — aggiungere `runWithConfig`, `computeStats`
6. `src/mosqueeze-bench/src/BenchmarkRunner.cpp` — implementazione
7. `src/mosqueeze-bench/src/main.cpp` — nuovo CLI
8. `src/mosqueeze-bench/CMakeLists.txt` — aggioranre sorgenti
9. `src/libmosqueeze/CMakeLists.txt` — aggiungere `BenchmarkConfig.hpp`

---

## Note Tecniche

1. **Backward Compatibility**: Mantenere `run()` e `runGrid()` API esistenti
2. **Memory Tracking**: `platform::getPeakMemoryUsage()` già implementato
3. **Cancellation**: Usare `std::atomic<bool>` per thread-safety
4. **Progress Rate limiting**: Non chiamare callback più di 10 volte al secondo
5. **Large Files**: Gestire file >2GB con streaming (già supportato dagli engine)

---

## Collegamenti

- Issue #2 (Benchmark Harness) — implementazione iniziale
- Issue #9 (LZMA/Brotli) — engine aggiunti
- Issue #10 (ZPAQ) — engine aggiunto
- Issue #21 (Video Support) — test su video