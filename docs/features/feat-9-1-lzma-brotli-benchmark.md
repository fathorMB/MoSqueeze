# Worker Spec: LZMA/Brotli Engines + Benchmark Harness (Issues #9 + #1)

## Overview

Implementare i motori LZMA/XZ e Brotli (Issue #9) e la foundation del benchmark harness (Issue #1) in un'unica branch. I motori sono prerequisiti per il harness.

## Stato Attuale

Il branch `feat/9-1-lzma-brotli-benchmark` parte da main dopo il merge di #7+#8. Il worker troverà:

- `src/libmosqueeze/include/mosqueeze/ICompressionEngine.hpp` — interfaccia completa
- `src/libmosqueeze/include/mosqueeze/Types.hpp` — CompressionOptions, CompressionResult, FileType
- `src/libmosqueeze/src/engines/ZstdEngine.cpp` — riferimento per pattern streaming
- `src/mosqueeze-bench/src/main.cpp` — placeholder vuoto
- `vcpkg.json` — già include `liblzma`, `brotli`, `zstd`, `cli11`, `fmt`, `spdlog`, `nlohmann-json`

## Obiettivo

1. LzmaEngine implementato con streaming API
2. BrotliEngine implementato con streaming API
3. BenchmarkRunner base funzionante
4. CorpusManager per gestione file di test
5. ResultsStore con SQLite + JSON export
6. Test per entrambi i motori

---

## Parte 1: LZMA/XZ Engine (Issue #9)

### File da creare

```
src/libmosqueeze/include/mosqueeze/engines/
└── LzmaEngine.hpp

src/libmosqueeze/src/engines/
└── LzmaEngine.cpp
```

### Requisiti Implementativi

#### LzmaEngine.hpp

```cpp
#pragma once
#include <mosqueeze/ICompressionEngine.hpp>
#include <vector>

namespace mosqueeze {

class LzmaEngine : public ICompressionEngine {
public:
    std::string name() const override { return "xz"; }
    std::string fileExtension() const override { return ".xz"; }
    std::vector<int> supportedLevels() const override;
    int defaultLevel() const override { return 9; }  // Max per cold storage
    int maxLevel() const override { return 9; }

    CompressionResult compress(
        std::istream& input,
        std::ostream& output,
        const CompressionOptions& opts = {}) override;

    void decompress(
        std::istream& input,
        std::ostream& output) override;
};

} // namespace mosqueeze
```

#### LzmaEngine.cpp — Requisiti Streaming

**CRITICO: Usare liblzma streaming API, mai caricare tutto in memoria.**

```cpp
// Pseudocodice compressione streaming
#include <lzma.h>

static constexpr size_t BUFFER_SIZE = 64 * 1024;

CompressionResult LzmaEngine::compress(...) {
    // Livello: opts.level se specificato (0-9), altrimenti 9
    // Se opts.extreme: usare LZMA_PRESET_EXTREME (level | LZMA_PRESET_EXTREME_FLAG)
    
    // Inizializzare lzma_stream con lzma_easy_encoder()
    // LZMA_CHECK_CRC64 per integrità (standard XZ)
    
    // Loop: leggere 64KB → lzma_code(LZMA_RUN) → scrivere output
    // Finalizzare con lzma_code(LZMA_FINISH)
    
    // Tracciare: originalBytes, compressedBytes, duration, peakMemoryBytes
}

void LzmaEngine::decompress(...) {
    // Inizializzare lzma_stream con lzma_stream_decoder()
    // Loop: leggere chunk → lzma_code(LZMA_RUN) → scrivere output
    // Gestire LZMA_STREAM_END per terminare
}
```

**Note liblzma:**
- Usare `lzma_easy_encoder()` per semplicità (gestisce filtri automaticamente)
- `LZMA_CHECK_CRC64` è lo standard per XZ
- `LZMA_PRESET_EXTREME` migliora ratio ma è molto lento (ok per cold storage)
- Link CMake: `lzma` o `LibLZMA::LibLZMA`

---

## Parte 2: Brotli Engine (Issue #9)

### File da creare

```
src/libmosqueeze/include/mosqueeze/engines/
└── BrotliEngine.hpp

src/libmosqueeze/src/engines/
└── BrotliEngine.cpp
```

### Requisiti Implementativi

#### BrotliEngine.hpp

```cpp
#pragma once
#include <mosqueeze/ICompressionEngine.hpp>
#include <vector>

namespace mosqueeze {

class BrotliEngine : public ICompressionEngine {
public:
    std::string name() const override { return "brotli"; }
    std::string fileExtension() const override { return ".br"; }
    std::vector<int> supportedLevels() const override;
    int defaultLevel() const override { return 11; }  // Max per cold storage
    int maxLevel() const override { return 11; }

    CompressionResult compress(
        std::istream& input,
        std::ostream& output,
        const CompressionOptions& opts = {}) override;

    void decompress(
        std::istream& input,
        std::ostream& output) override;
};

} // namespace mosqueeze
```

#### BrotliEngine.cpp — Requisiti Streaming

**CRITICO: Usare Brotli streaming API con BrotliEncoderState.**

```cpp
// Pseudocodice compressione streaming
#include <brotli/encode.h>
#include <brotli/decode.h>

static constexpr size_t BUFFER_SIZE = 64 * 1024;

CompressionResult BrotliEngine::compress(...) {
    // Quality: opts.level se specificato (0-11), altrimenti 11
    // Se opts.extreme: già al massimo (quality 11)
    
    // BrotliEncoderCreateInstance(nullptr, nullptr, nullptr)
    // BrotliEncoderSetParameter(quality)
    
    // Loop: leggere chunk → BrotliEncoderCompressStream → disponibile output?
    // Finalizzare con BROTLI_OPERATION_FINISH
    
    // Tracciare metriche come gli altri engine
}

void BrotliEngine::decompress(...) {
    // BrotliDecoderCreateInstance()
    // Loop: leggere chunk → BrotliDecoderDecompressStream → scrivere output
}
```

**Note Brotli:**
- Quality 11 è il massimo, molto lento ma ottimo ratio per testo
- Brotli è ottimizzato per text/web content (JSON, XML, HTML, codice sorgente)
- Link CMake: `brotli::brotli` o `brotli` (provare entrambi)

---

## Parte 3: Aggiornamenti CMake

### src/libmosqueeze/CMakeLists.txt

```cmake
# Aggiungere i nuovi engine ai sorgenti
add_library(libmosqueeze SHARED
  src/Version.cpp
  src/platform/Platform.cpp
  src/engines/ZstdEngine.cpp
  src/engines/LzmaEngine.cpp      # NUOVO
  src/engines/BrotliEngine.cpp    # NUOVO
  src/FileTypeDetector.cpp
)

# Aggiungere dipendenze
find_package(LibLZMA CONFIG REQUIRED)
find_package(brotli CONFIG REQUIRED)

target_link_libraries(libmosqueeze
  PRIVATE
    zstd::libzstd_shared
    LibLZMA::LibLZMA
    brotli::brotli
)
```

---

## Parte 4: Benchmark Harness Foundation (Issue #1)

### Struttura Directory

```
src/mosqueeze-bench/
├── include/mosqueeze/bench/
│   ├── BenchmarkRunner.hpp
│   ├── CorpusManager.hpp
│   └── ResultsStore.hpp
└── src/
    ├── main.cpp          (CLI entry point)
    ├── BenchmarkRunner.cpp
    ├── CorpusManager.cpp
    └── ResultsStore.cpp
```

### BenchmarkRunner.hpp

```cpp
#pragma once
#include <mosqueeze/ICompressionEngine.hpp>
#include <mosqueeze/Types.hpp>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace mosqueeze::bench {

struct BenchmarkResult {
    std::string algorithm;
    int level = 0;
    std::filesystem::path file;
    FileType fileType = FileType::Unknown;
    size_t originalBytes = 0;
    size_t compressedBytes = 0;
    std::chrono::milliseconds encodeTime{0};
    std::chrono::milliseconds decodeTime{0};
    size_t peakMemoryBytes = 0;

    double ratio() const {
        return compressedBytes > 0
            ? static_cast<double>(originalBytes) / compressedBytes
            : 0.0;
    }
};

class BenchmarkRunner {
public:
    BenchmarkRunner();

    // Aggiungere engine registrati
    void registerEngine(std::unique_ptr<ICompressionEngine> engine);

    // Eseguire benchmark su lista file
    std::vector<BenchmarkResult> run(
        const std::vector<std::filesystem::path>& files,
        const std::vector<std::string>& algorithms = {},
        const std::vector<int>& levels = {});

    // Grid search: tutti gli algoritmi × tutti i livelli
    std::vector<BenchmarkResult> runGrid(
        const std::vector<std::filesystem::path>& files);

private:
    std::vector<std::unique_ptr<ICompressionEngine>> engines_;
    ICompressionEngine* findEngine(const std::string& name) const;
};

} // namespace mosqueeze::bench
```

### CorpusManager.hpp

```cpp
#pragma once
#include <mosqueeze/Types.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace mosqueeze::bench {

struct CorpusFile {
    std::filesystem::path path;
    FileType type = FileType::Unknown;
    std::string description;
    size_t sizeBytes = 0;
};

class CorpusManager {
public:
    CorpusManager();

    // Directory standard del corpus
    std::filesystem::path corpusDirectory() const;

    // Lista file nel corpus
    std::vector<CorpusFile> listFiles(FileType filter = FileType::Unknown) const;

    // Aggiungere file al corpus
    void addFile(const std::filesystem::path& file, FileType type, const std::string& description = "");

    // Scaricare corpus standard (se non esiste)
    void downloadStandardCorpus();

    // Statistiche
    size_t totalFiles() const;
    size_t totalSize() const;

private:
    std::filesystem::path corpusDir_;
    std::vector<CorpusFile> files_;
};

} // namespace mosqueeze::bench
```

### ResultsStore.hpp

```cpp
#pragma once
#include <mosqueeze/bench/BenchmarkRunner.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace mosqueeze::bench {

class ResultsStore {
public:
    ResultsStore(const std::filesystem::path& dbPath = ":memory:");

    // Salvare risultati
    void save(const BenchmarkResult& result);
    void saveAll(const std::vector<BenchmarkResult>& results);

    // Query
    std::vector<BenchmarkResult> query(const std::string& whereClause = "");

    // Export
    void exportCsv(const std::filesystem::path& output);
    void exportJson(const std::filesystem::path& output);

    // Clear
    void clear();

private:
    std::filesystem::path dbPath_;
    void initDatabase();

    // SQLite handle gestito internamente (implementazione privata)
};

} // namespace mosqueeze::bench
```

### CMake per mosqueeze-bench

```cmake
# src/mosqueeze-bench/CMakeLists.txt

find_package(SQLite3 CONFIG REQUIRED)

add_executable(mosqueeze-bench
  src/main.cpp
  src/BenchmarkRunner.cpp
  src/CorpusManager.cpp
  src/ResultsStore.cpp
)

target_link_libraries(mosqueeze-bench
  PRIVATE
    libmosqueeze
    SQLite::SQLite3
    CLI11::CLI11
    fmt::fmt
    spdlog::spdlog
)

target_include_directories(mosqueeze-bench
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
```

---

## Parte 5: Test

### tests/unit/LzmaEngine_test.cpp

```cpp
#include <mosqueeze/engines/LzmaEngine.hpp>
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    mosqueeze::LzmaEngine engine;

    // Roundtrip test
    std::string original = "Hello, LZMA compression test. Binary data: ";
    for (int i = 0; i < 256; ++i) original += static_cast<char>(i);
    original += original;

    std::istringstream input(original);
    std::ostringstream compressed;

    auto result = engine.compress(input, compressed);
    assert(result.ratio() > 1.0);
    assert(result.compressedBytes < result.originalBytes);

    std::istringstream compressedInput(compressed.str());
    std::ostringstream decompressed;
    engine.decompress(compressedInput, decompressed);

    assert(decompressed.str() == original);

    // Metadata
    auto levels = engine.supportedLevels();
    assert(levels.size() == 10); // 0-9
    assert(engine.name() == "xz");
    assert(engine.fileExtension() == ".xz");
    assert(engine.defaultLevel() == 9);

    std::cout << "[PASS] LzmaEngine roundtrip OK\n";
    return 0;
}
```

### tests/unit/BrotliEngine_test.cpp

```cpp
#include <mosqueeze/engines/BrotliEngine.hpp>
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    mosqueeze::BrotliEngine engine;

    // Roundtrip test - Brotli preferisce testo
    std::string original = R"(
{
    "name": "test",
    "data": [1, 2, 3, 4, 5],
    "nested": {"key": "value"}
}
)";
    for (int i = 0; i < 100; ++i) original += original;

    std::istringstream input(original);
    std::ostringstream compressed;

    auto result = engine.compress(input, compressed);
    assert(result.ratio() > 1.0);
    assert(result.compressedBytes < result.originalBytes);

    std::istringstream compressedInput(compressed.str());
    std::ostringstream decompressed;
    engine.decompress(compressedInput, decompressed);

    assert(decompressed.str() == original);

    // Metadata
    auto levels = engine.supportedLevels();
    assert(levels.size() == 12); // 0-11
    assert(engine.name() == "brotli");
    assert(engine.fileExtension() == ".br");
    assert(engine.defaultLevel() == 11);

    std::cout << "[PASS] BrotliEngine roundtrip OK\n";
    return 0;
}
```

---

## Parte 6: CLI Entry Point (mosqueeze-bench)

```cpp
// src/mosqueeze-bench/src/main.cpp
#include <mosqueeze/bench/BenchmarkRunner.hpp>
#include <mosqueeze/bench/CorpusManager.hpp>
#include <mosqueeze/bench/ResultsStore.hpp>
#include <mosqueeze/engines/ZstdEngine.hpp>
#include <mosqueeze/engines/LzmaEngine.hpp>
#include <mosqueeze/engines/BrotliEngine.hpp>

#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>

int main(int argc, char* argv[]) {
    CLI::App app{"MoSqueeze Benchmark Harness", "mosqueeze-bench"};

    std::string corpusPath = "corpus/";
    std::string outputPath = "results/";
    bool json = true;
    bool csv = false;

    app.add_option("-c,--corpus", corpusPath, "Path to corpus directory");
    app.add_option("-o,--output", outputPath, "Output directory for results");
    app.add_flag("--json", json, "Export results as JSON");
    app.add_flag("--csv", csv, "Export results as CSV");

    CLI11_PARSE(app, argc, argv);

    // Setup
    mosqueeze::bench::BenchmarkRunner runner;
    runner.registerEngine(std::make_unique<mosqueeze::ZstdEngine>());
    runner.registerEngine(std::make_unique<mosqueeze::LzmaEngine>());
    runner.registerEngine(std::make_unique<mosqueeze::BrotliEngine>());

    mosqueeze::bench::CorpusManager corpus(corpusPath);
    mosqueeze::bench::ResultsStore store;

    std::cout << "MoSqueeze Benchmark Harness v0.1.0\n";
    std::cout << "Corpus: " << corpus.totalFiles() << " files (" 
              << corpus.totalSize() << " bytes)\n";

    // Run benchmark
    auto files = corpus.listFiles();
    std::vector<std::filesystem::path> paths;
    for (const auto& f : files) {
        paths.push_back(f.path);
    }

    auto results = runner.runGrid(paths);
    store.saveAll(results);

    // Export
    std::filesystem::create_directories(outputPath);
    if (json) {
        store.exportJson(outputPath + "/results.json");
        std::cout << "Exported JSON to " << outputPath << "/results.json\n";
    }
    if (csv) {
        store.exportCsv(outputPath + "/results.csv");
        std::cout << "Exported CSV to " << outputPath << "/results.csv\n";
    }

    // Summary
    std::cout << "\n=== Top 5 by Ratio ===\n";
    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        return a.ratio() > b.ratio();
    });
    for (size_t i = 0; i < std::min(results.size(), size_t(5)); ++i) {
        const auto& r = results[i];
        std::cout << r.algorithm << "/" << r.level << ": "
                  << r.ratio() << "x (" << r.file.filename().string() << ")\n";
    }

    return 0;
}
```

---

## Vincoli e Note

1. **Streaming solo**: Mai caricare intero file in memoria. Sempre 64KB buffer.
2. **LZMA XZ format**: Usare `lzma_easy_encoder()` con `LZMA_CHECK_CRC64` per compatibilità standard.
3. **Brotli quality 11**: Il default per cold storage. Molto lento ma ratio ottimo per testo.
4. **SQLite**: Usare `sqlite3` vcpkg package. Se non disponibile, usare amalgamation .c/.h inline.
5. **Memory tracking**: Usare `platform::getPeakMemoryUsage()` come in ZstdEngine.
6. **Error handling**: Sempre `std::runtime_error` con messaggi descrittivi.
7. **Cross-platform**: Testare su Windows, Linux, macOS.

---

## Definizione di Fatto

- [ ] `LzmaEngine.hpp/cpp` implementa compressione/decompressione streaming XZ
- [ ] `BrotliEngine.hpp/cpp` implementa compressione/decompressione streaming Brotli
- [ ] `LzmaEngine_test.cpp` passa (roundtrip, metadati corretti)
- [ ] `BrotliEngine_test.cpp` passa (roundtrip, metadati corretti)
- [ ] `BenchmarkRunner.hpp/cpp` può eseguire benchmark su lista file
- [ ] `CorpusManager.hpp/cpp` gestisce file di test
- [ ] `ResultsStore.hpp/cpp` salva risultati in SQLite e exporta JSON/CSV
- [ ] CLI `mosqueeze-bench` funziona con `--corpus` e `--output`
- [ ] `src/libmosqueeze/CMakeLists.txt` aggiornato con LzmaEngine e BrotliEngine
- [ ] `src/mosqueeze-bench/CMakeLists.txt` aggiornato con SQLite3
- [ ] CI passa su Linux, macOS, Windows
- [ ] Nessun warning con `-Wall -Wextra`