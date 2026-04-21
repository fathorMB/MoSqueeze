# Worker Spec: Algorithm Selection Engine (Issue #11)

## Overview

Implementare l'Algorithm Selection Engine che, dato un file e la sua classificazione, raccomanda l'algoritmo e livello ottimale per cold storage.

## Stato Attuale

Il branch `feat/11-algorithm-selection` parte da main con:
- `FileTypeDetector` completo (magic bytes, extension, text sniffing)
- `FileClassification` struct con `type`, `isCompressed`, `canRecompress`, `recommendation`
- `ICompressionEngine` interface con metadati (`name`, `supportedLevels`, `defaultLevel`, `maxLevel`)
- Engines implementati: `ZstdEngine`, `LzmaEngine`, `BrotliEngine`
- `bench::BenchmarkResult` per dati storici

## Obiettivo

1. `AlgorithmSelector` che raccomanda algoritmo + livello
2. Selezione rule-based con fallback gerarchico
3. Supporto per benchmark data-driven (opzionale)
4. Configurazione JSON per regole custom
5. CLI `mosqueeze analyze` command

---

## Parte 1: AlgorithmSelector

### File da creare

```
src/libmosqueeze/include/mosqueeze/
└── AlgorithmSelector.hpp

src/libmosqueeze/src/
└── AlgorithmSelector.cpp
```

### AlgorithmSelector.hpp

```cpp
#pragma once

#include <mosqueeze/FileClassification.hpp>
#include <mosqueeze/ICompressionEngine.hpp>
#include <mosqueeze/Types.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mosqueeze {

// Forward declaration for bench integration
namespace bench {
struct BenchmarkResult;
}

struct Selection {
    std::string algorithm;         // "zstd", "lzma", "brotli"
    int level = 0;                  // Algorithm-specific level
    bool shouldSkip = false;        // Already compressed, skip
    std::string rationale;         // Human-readable explanation
    std::string fallbackAlgorithm;  // Alternative if primary fails
    int fallbackLevel = 0;
};

class AlgorithmSelector {
public:
    AlgorithmSelector();

    // Main selection method
    Selection select(
        const FileClassification& classification,
        const std::filesystem::path& file = {}) const;

    // With benchmark data for data-driven selection
    void setBenchmarkData(const std::vector<bench::BenchmarkResult>& results);
    void clearBenchmarkData();

    // Load/save rules from JSON
    bool loadRules(const std::filesystem::path& configPath);
    bool saveRules(const std::filesystem::path& configPath) const;

    // Register available engines (for level validation)
    void registerEngine(const ICompressionEngine* engine);

    // Get all available algorithms
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

    std::vector<SelectionRule> rules_;
    std::unordered_map<FileType, SelectionRule> typeToRule_;
    
    // Benchmark data for data-driven selection (optional)
    std::unordered_map<std::string, std::vector<bench::BenchmarkResult>> benchmarkByFileType_;
    bool hasBenchmarkData_ = false;

    // Registered engines for validation
    std::unordered_map<std::string, const ICompressionEngine*> engines_;

    void initializeDefaultRules();
    Selection selectFromRules(const FileClassification& classification) const;
    Selection selectFromBenchmark(const FileClassification& classification) const;
};

} // namespace mosqueeze
```

### AlgorithmSelector.cpp — Implementazione

```cpp
#include <mosqueeze/AlgorithmSelector.hpp>

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace mosqueeze {

AlgorithmSelector::AlgorithmSelector() {
    initializeDefaultRules();
}

void AlgorithmSelector::initializeDefaultRules() {
    // Cold storage optimized rules
    
    // TEXT FILES
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
    
    // IMAGES
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
    
    // VIDEO - All skip
    typeToRule_[FileType::Video_MP4] = {
        FileType::Video_MP4,
        "", 0, "MP4: H.264/265 already compressed, skip",
        true, "", 0
    };
    
    typeToRule_[FileType::Video_MKV] = {
        FileType::Video_MKV,
        "", 0, "MKV: container with compressed streams, skip",
        true, "", 0
    };
    
    typeToRule_[FileType::Video_WebM] = {
        FileType::Video_WebM,
        "", 0, "WebM: VP9/AV1 compressed, skip",
        true, "", 0
    };
    
    // AUDIO
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
    
    // BINARY
    typeToRule_[FileType::Binary_Executable] = {
        FileType::Binary_Executable,
        "lzma", 9, "ELF/EXE: LZMA excels on structured binary",
        false, "zstd", 22
    };
    
    typeToRule_[FileType::Binary_Database] = {
        FileType::Binary_Database,
        "lzma", 9, "Database: LZMA for repetitive table structures",
        false, "zstd", 22
    };
    
    typeToRule_[FileType::Binary_Chunked] = {
        FileType::Binary_Chunked,
        "zstd", 22, "Generic binary: zstd as safe default",
        false, "lzma", 9
    };
    
    // ARCHIVES
    typeToRule_[FileType::Archive_ZIP] = {
        FileType::Archive_ZIP,
        "", 0, "ZIP: extract and recompress contents individually",
        true, "", 0  // skip, requires extraction first
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
    
    // UNKNOWN
    typeToRule_[FileType::Unknown] = {
        FileType::Unknown,
        "zstd", 22, "Unknown: zstd as safe general-purpose default",
        false, "lzma", 9
    };
}

Selection AlgorithmSelector::select(
    const FileClassification& classification, 
    const std::filesystem::path& file) const {
    
    // 1. Check if we should skip
    if (!classification.canRecompress || classification.recommendation == "skip") {
        Selection skipSel;
        skipSel.shouldSkip = true;
        skipSel.rationale = "File type marked as skip: " + classification.mimeType;
        return skipSel;
    }
    
    // 2. Check for extract-then-compress
    if (classification.recommendation == "extract-then-compress") {
        Selection extractSel;
        extractSel.shouldSkip = true;
        extractSel.rationale = "Archive detected: extract contents first, then compress individually";
        return extractSel;
    }
    
    // 3. If benchmark data available, use data-driven selection
    if (hasBenchmarkData_) {
        auto benchSel = selectFromBenchmark(classification);
        if (!benchSel.algorithm.empty()) {
            return benchSel;
        }
    }
    
    // 4. Fall back to rule-based selection
    return selectFromRules(classification);
}

Selection AlgorithmSelector::selectFromRules(
    const FileClassification& classification) const {
    
    auto it = typeToRule_.find(classification.type);
    if (it == typeToRule_.end()) {
        // Unknown type, use default
        Selection defSel;
        defSel.algorithm = "zstd";
        defSel.level = 22;
        defSel.rationale = "Unknown file type: using zstd as safe default";
        defSel.fallbackAlgorithm = "lzma";
        defSel.fallbackLevel = 9;
        return defSel;
    }
    
    const auto& rule = it->second;
    
    Selection sel;
    sel.algorithm = rule.algorithm;
    sel.level = rule.level;
    sel.shouldSkip = rule.skip;
    sel.rationale = rule.rationale;
    sel.fallbackAlgorithm = rule.fallbackAlgorithm;
    sel.fallbackLevel = rule.fallbackLevel;
    
    return sel;
}

Selection AlgorithmSelector::selectFromBenchmark(
    const FileClassification& classification) const {
    
    // Find best algorithm for this file type from benchmark data
    std::string typeKey = std::to_string(static_cast<int>(classification.type));
    auto it = benchmarkByFileType_.find(typeKey);
    if (it == benchmarkByFileType_.end()) {
        return {}; // No data for this type
    }
    
    // Find best ratio from benchmarks
    const auto& results = it->second;
    if (results.empty()) {
        return {};
    }
    
    // Sort by ratio descending
    std::vector<std::pair<std::string, int>> ranked;
    for (const auto& r : results) {
        ranked.push_back({r.algorithm, r.level});
    }
    
    // Find winner
    auto best = std::max_element(results.begin(), results.end(),
        [](const bench::BenchmarkResult& a, const bench::BenchmarkResult& b) {
            return a.ratio() < b.ratio();
        });
    
    if (best == results.end()) {
        return {};
    }
    
    Selection sel;
    sel.algorithm = best->algorithm;
    sel.level = best->level;
    sel.rationale = "Benchmark-optimized: " + best->algorithm + " achieved " + 
                    std::to_string(best->ratio()) + "x ratio on similar files";
    
    // Find second best for fallback
    if (results.size() > 1) {
        auto second = std::max_element(results.begin(), results.end(),
            [&best](const bench::BenchmarkResult& a, const bench::BenchmarkResult& b) {
                if (&a == &*best) return true;
                if (&b == &*best) return false;
                return a.ratio() < b.ratio();
            });
        if (second != results.end() && second != best) {
            sel.fallbackAlgorithm = second->algorithm;
            sel.fallbackLevel = second->level;
        }
    }
    
    return sel;
}

void AlgorithmSelector::registerEngine(const ICompressionEngine* engine) {
    if (engine) {
        engines_[engine->name()] = engine;
    }
}

std::vector<std::string> AlgorithmSelector::availableAlgorithms() const {
    std::vector<std::string> names;
    names.reserve(engines_.size());
    for (const auto& [name, engine] : engines_) {
        names.push_back(name);
    }
    return names;
}

// JSON config support
bool AlgorithmSelector::loadRules(const std::filesystem::path& configPath) {
    std::ifstream f(configPath);
    if (!f) return false;
    
    try {
        nlohmann::json j;
        f >> j;
        
        if (j.contains("rules") && j["rules"].is_array()) {
            for (const auto& ruleJson : j["rules"]) {
                SelectionRule rule;
                rule.fileType = static_cast<FileType>(ruleJson.value("fileType", 0));
                rule.algorithm = ruleJson.value("algorithm", "");
                rule.level = ruleJson.value("level", 0);
                rule.rationale = ruleJson.value("rationale", "");
                rule.skip = ruleJson.value("skip", false);
                rule.fallbackAlgorithm = ruleJson.value("fallbackAlgorithm", "");
                rule.fallbackLevel = ruleJson.value("fallbackLevel", 0);
                
                typeToRule_[rule.fileType] = rule;
            }
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

bool AlgorithmSelector::saveRules(const std::filesystem::path& configPath) const {
    std::ofstream f(configPath);
    if (!f) return false;
    
    nlohmann::json j;
    j["rules"] = nlohmann::json::array();
    
    for (const auto& [type, rule] : typeToRule_) {
        nlohmann::json ruleJson;
        ruleJson["fileType"] = static_cast<int>(type);
        ruleJson["algorithm"] = rule.algorithm;
        ruleJson["level"] = rule.level;
        ruleJson["rationale"] = rule.rationale;
        ruleJson["skip"] = rule.skip;
        ruleJson["fallbackAlgorithm"] = rule.fallbackAlgorithm;
        ruleJson["fallbackLevel"] = rule.fallbackLevel;
        j["rules"].push_back(ruleJson);
    }
    
    f << j.dump(2);
    return true;
}

void AlgorithmSelector::setBenchmarkData(const std::vector<bench::BenchmarkResult>& results) {
    // Group by file type
    for (const auto& r : results) {
        std::string key = std::to_string(static_cast<int>(r.fileType));
        benchmarkByFileType_[key].push_back(r);
    }
    hasBenchmarkData_ = true;
}

void AlgorithmSelector::clearBenchmarkData() {
    benchmarkByFileType_.clear();
    hasBenchmarkData_ = false;
}

} // namespace mosqueeze
```

---

## Parte 2: Aggiornamenti CMake

### src/libmosqueeze/CMakeLists.txt

```cmake
add_library(libmosqueeze SHARED
  src/Version.cpp
  src/platform/Platform.cpp
  src/engines/ZstdEngine.cpp
  src/engines/LzmaEngine.cpp
  src/engines/BrotliEngine.cpp
  src/FileTypeDetector.cpp
  src/AlgorithmSelector.cpp        # NUOVO
)

# Already has nlohmann-json for JSON support
```

---

## Parte 3: CLI Integration

### Aggiornare mosqueeze-cli

`src/mosqueeze-cli/src/main.cpp`:

```cpp
#include <mosqueeze/AlgorithmSelector.hpp>
#include <mosqueeze/FileTypeDetector.hpp>
#include <mosqueeze/engines/ZstdEngine.hpp>
#include <mosqueeze/engines/LzmaEngine.hpp>
#include <mosqueeze/engines/BrotliEngine.hpp>

// Add 'analyze' subcommand
void addAnalyzeCommand(CLI::App& app) {
    auto analyze = app.add_subcommand("analyze", "Analyze file and recommend algorithm");
    
    std::string inputFile;
    bool verbose = false;
    bool benchmark = false;
    
    analyze->add_option("file", inputFile, "File to analyze")->required();
    analyze->add_flag("-v,--verbose", verbose, "Show detailed analysis");
    analyze->add_flag("-b,--benchmark", benchmark, "Run quick benchmark");
    
    analyze->callback([&]() {
        std::filesystem::path path(inputFile);
        
        // Detect file type
        mosqueeze::FileTypeDetector detector;
        auto classification = detector.detect(path);
        
        // Get recommendation
        mosqueeze::AlgorithmSelector selector;
        selector.registerEngine(&mosqueeze::ZstdEngine{});
        selector.registerEngine(&mosqueeze::LzmaEngine{});
        selector.registerEngine(&mosqueeze::BrotliEngine{});
        
        auto selection = selector.select(classification, path);
        
        // Output
        fmt::print("File: {}\n", path.string());
        fmt::print("Type: {}\n", classification.mimeType);
        fmt::print("Compressed: {}\n", classification.isCompressed ? "yes" : "no");
        
        if (verbose) {
            fmt::print("FileType enum: {}\n", static_cast<int>(classification.type));
            fmt::print("Can recompress: {}\n", classification.canRecompress ? "yes" : "no");
        }
        
        fmt::print("\nRecommendation:\n");
        
        if (selection.shouldSkip) {
            fmt::print("  Action: SKIP\n");
            fmt::print("  Reason: {}\n", selection.rationale);
        } else {
            fmt::print("  Algorithm: {} level {}\n", selection.algorithm, selection.level);
            fmt::print("  Fallback: {} level {}\n", selection.fallbackAlgorithm, selection.fallbackLevel);
            fmt::print("  Reason: {}\n", selection.rationale);
            
            if (benchmark) {
                // Run quick benchmark (30s limit per algorithm)
                fmt::print("\nRunning quick benchmark...\n");
                // ... benchmark code ...
            }
        }
    });
}
```

---

## Parte 4: Test

### tests/unit/AlgorithmSelector_test.cpp

```cpp
#include <mosqueeze/AlgorithmSelector.hpp>
#include <mosqueeze/FileTypeDetector.hpp>

#include <cassert>
#include <iostream>

using namespace mosqueeze;

void testTextSourceCode() {
    FileTypeDetector detector;
    AlgorithmSelector selector;
    
    // Create temp file
    std::ofstream f("test.cpp");
    f << "// Test source file\nint main() { return 0; }\n";
    f.close();
    
    auto classification = detector.detect("test.cpp");
    auto selection = selector.select(classification, "test.cpp");
    
    assert(!selection.shouldSkip);
    assert(selection.algorithm == "zstd");
    assert(selection.level == 22);
    assert(!selection.rationale.empty());
    
    std::filesystem::remove("test.cpp");
    std::cout << "[PASS] Text_SourceCode selection\n";
}

void testJsonFile() {
    FileClassification classification;
    classification.type = FileType::Text_Structured;
    classification.mimeType = "application/json";
    classification.isCompressed = false;
    classification.canRecompress = true;
    
    AlgorithmSelector selector;
    auto selection = selector.select(classification);
    
    assert(!selection.shouldSkip);
    assert(selection.algorithm == "brotli");
    assert(selection.level == 11);
    
    std::cout << "[PASS] JSON selection\n";
}

void testSkipJpeg() {
    FileClassification classification;
    classification.type = FileType::Image_JPEG;
    classification.mimeType = "image/jpeg";
    classification.isCompressed = true;
    classification.canRecompress = false;
    classification.recommendation = "skip";
    
    AlgorithmSelector selector;
    auto selection = selector.select(classification);
    
    assert(selection.shouldSkip);
    assert(selection.rationale.find("skip") != std::string::npos);
    
    std::cout << "[PASS] JPEG skip\n";
}

void testBinaryExecutable() {
    FileClassification classification;
    classification.type = FileType::Binary_Executable;
    classification.mimeType = "application/x-elf";
    classification.isCompressed = false;
    classification.canRecompress = true;
    
    AlgorithmSelector selector;
    auto selection = selector.select(classification);
    
    assert(!selection.shouldSkip);
    assert(selection.algorithm == "lzma");
    assert(selection.level == 9);
    assert(selection.fallbackAlgorithm == "zstd");
    
    std::cout << "[PASS] Binary executable selection\n";
}

void testArchiveExtract() {
    FileClassification classification;
    classification.type = FileType::Archive_ZIP;
    classification.mimeType = "application/zip";
    classification.isCompressed = true;
    classification.canRecompress = false;
    classification.recommendation = "extract-then-compress";
    
    AlgorithmSelector selector;
    auto selection = selector.select(classification);
    
    assert(selection.shouldSkip);
    assert(selection.rationale.find("extract") != std::string::npos);
    
    std::cout << "[PASS] Archive extract recommendation\n";
}

void testUnknownFile() {
    FileClassification classification;
    classification.type = FileType::Unknown;
    
    AlgorithmSelector selector;
    auto selection = selector.select(classification);
    
    assert(!selection.shouldSkip);
    assert(selection.algorithm == "zstd");
    assert(selection.level == 22);
    
    std::cout << "[PASS] Unknown fallback\n";
}

void testJsonConfig() {
    AlgorithmSelector selector;
    
    // Save default rules
    assert(selector.saveRules("test_rules.json"));
    
    // Load them back
    assert(selector.loadRules("test_rules.json"));
    
    std::filesystem::remove("test_rules.json");
    std::cout << "[PASS] JSON config save/load\n";
}

int main() {
    testTextSourceCode();
    testJsonFile();
    testSkipJpeg();
    testBinaryExecutable();
    testArchiveExtract();
    testUnknownFile();
    testJsonConfig();
    
    std::cout << "[PASS] All AlgorithmSelector tests passed\n";
    return 0;
}
```

### tests/CMakeLists.txt

```cmake
add_executable(AlgorithmSelector_test
  unit/AlgorithmSelector_test.cpp
)

target_link_libraries(AlgorithmSelector_test
  PRIVATE
    libmosqueeze
)

add_test(NAME AlgorithmSelector_test COMMAND AlgorithmSelector_test)
```

---

## Parte 5: JSON Config Format

### Esempio: `config/selection_rules.json`

```json
{
  "$schema": "https://example.com/schemas/selection-rules.json",
  "version": 1,
  "rules": [
    {
      "fileType": 0,
      "algorithm": "zstd",
      "level": 22,
      "rationale": "Source code: zstd level 22 for cold storage",
      "skip": false,
      "fallbackAlgorithm": "brotli",
      "fallbackLevel": 11
    },
    {
      "fileType": 7,
      "algorithm": "",
      "level": 0,
      "rationale": "JPEG: already compressed, skip",
      "skip": true,
      "fallbackAlgorithm": "",
      "fallbackLevel": 0
    }
  ]
}
```

---

## Selection Matrix (Summary)

| FileType | Algorithm | Level | Skip? | Rationale |
|----------|-----------|-------|-------|-----------|
| Text_SourceCode | zstd | 22 | No | Best for code, fast decode |
| Text_Structured | brotli | 11 | No | Optimized for JSON/XML |
| Text_Prose | brotli | 11 | No | Natural language |
| Text_Log | lzma | 9 | No | Repetitive patterns |
| Image_PNG | zstd | 19 | No | Can improve on zlib |
| Image_JPEG | — | — | **Yes** | Lossy, skip |
| Image_WebP | — | — | **Yes** | Already compressed |
| Image_Raw | lzma | 9 | No | Pixel data |
| Video_* | — | — | **Yes** | Already compressed |
| Audio_WAV | lzma | 9 | No | Uncompressed PCM |
| Audio_MP3/FLAC | — | — | **Yes** | Already compressed |
| Binary_Executable | lzma | 9 | No | Structured binary |
| Binary_Database | lzma | 9 | No | Table patterns |
| Archive_ZIP/7Z | — | — | **Yes** | Extract first |
| Archive_TAR | zstd | 22 | No | Uncompressed |
| Unknown | zstd | 22 | No | Safe default |

---

## Definizione di Fatto

- [ ] `AlgorithmSelector.hpp/cpp` implementato con selecby by rules
- [ ] `select()` method restituisce `Selection` con algoritmo, livello, rationale
- [ ] Skip detection per JPEG, MP4, MP3, WebP, FLAC
- [ ] Fallback gerarchico per ogni FileType
- [ ] JSON config load/save funzionante
- [ ] `AlgorithmSelector_test.cpp` passa tutti i test
- [ ] CLI `mosqueeze analyze <file>` implementato
- [ ] CMakeLists.txt aggiornato
- [ ] CI passa su Linux, macOS, Windows

---

## Note Implementative

1. **Cold storage focus**: Tutti i livelli default sono massimi
2. **Fallback**: Ogni FileType ha un algoritmo di fallback se primary fallisce
3. **Benchmark integration**: `setBenchmarkData()` permette data-driven selection
4. **Extensibility**: JSON config permette override senza ricompilare
5. **Rationale**: Ogni selezione include spiegazione human-readable