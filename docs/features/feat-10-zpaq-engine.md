# Worker Spec: ZPAQ Engine (Issue #10)

## Overview

Integrare zpaq per massima compressione in cold storage. ZPAQ offre il miglior ratio disponibile ma è molto lento — ideale per archiviazione a lungo termine.

## Stato Attuale

Il branch `feat/10-zpaq-engine` parte da maincon:
- `ICompressionEngine` interface completa
- Engine esistenti: `ZstdEngine`, `LzmaEngine`, `BrotliEngine`
- Pattern streaming con 64KB buffer
- `vcpkg.json` con dipendenze esistenti (zstd, liblzma, brotli)

## Sfide ZPAQ

1. **Non in vcpkg** — Richiede vendor/build da source
2. **API file-based** — ZPAQ nativo opera su file, non stream
3. **GPL/PD license** — Verificare compatibilità
4. **Lentissimo encode** — Ma ideale per cold storage

---

## Parte 1: Analisi ZPAQ

### Cos'è ZPAQ

Creato da Matt Mahoney, ZPAQ usa:
- **Context mixing** — Combina predittori multipli
- **Neural network modeling** — Apprende pattern dai dati
- **Dictionary compression** — Su grande scala

### Repository Ufficiale

- https://github.com/zpaq/zpaq
- License: **Public Domain** (o GPL in alcune versioni)
- Format: Archive con journaling

### Levels/Methods

ZPAQ usa 5 "methods" predefiniti:
- **Method 1**: Fast, ~2.5x ratio
- **Method 2**: Balanced, ~3x ratio
- **Method 3**: Good, ~3.5x ratio
- **Method 4**: Better, ~4x ratio
- **Method 5**: Best, ~4.5-6x ratio (massimo)

Per cold storage: **Method 5 sempre**.

---

## Parte 2: Integrazione Options

### Option A: Vendored Source (RACCOMANDATO)

```
third_party/zpaq/
├── CMakeLists.txt
├── libzpaq/
│   ├── libzpaq.cpp
│   ├── libzpaq.h
│   └── CMakeLists.txt
└── zpaq.cpp (opzionale, solo per riferimento)
```

**Vantaggi**:
- Build deterministico
- Controllo versione
- Cross-platform

**Contro**:
- Codice extra nel repo
- Aggiornamenti manuali

### Option B: FetchContent

```cmake
FetchContent_Declare(
  zpaq
  GIT_REPOSITORY https://github.com/zpaq/zpaq.git
  GIT_TAG        v7.15
)
FetchContent_MakeAvailable(zpaq)
```

**Vantaggi**:
- Repository pulito
- Aggiornamenti semplici

**Contro**:
- Dipende da rete
- Meno controllo su build

### Option C: CLI Fallback

Shell out a `zpaq` binary se disponibile:

```cpp
CompressionResult ZpaqEngine::compress(...) {
    if (hasZpaqBinary()) {
        return compressViaBinary(input, output, opts);
    }
    throw std::runtime_error("zpaq binary not found");
}
```

**Vantaggi**:
- Semplice implementazione

**Contro**:
- Richiede installazione esterna
- Meno portabile

---

## Parte 3: ZpaqEngine Implementation

### File da creare

```
src/libmosqueeze/include/mosqueeze/engines/
└── ZpaqEngine.hpp

src/libmosqueeze/src/engines/
└── ZpaqEngine.cpp

third_party/zpaq/
├── CMakeLists.txt
└── libzpaq.cpp
```

### ZpaqEngine.hpp

```cpp
#pragma once

#include <mosqueeze/ICompressionEngine.hpp>
#include <vector>

namespace mosqueeze {

class ZpaqEngine : public ICompressionEngine {
public:
    ZpaqEngine();
    ~ZpaqEngine();

    std::string name() const override { return "zpaq"; }
    std::string fileExtension() const override { return ".zpaq"; }
    std::vector<int> supportedLevels() const override;
    int defaultLevel() const override { return 5; }
    int maxLevel() const override { return 5; }

    CompressionResult compress(
        std::istream& input,
        std::ostream& output,
        const CompressionOptions& opts = {}) override;

    void decompress(
        std::istream& input,
        std::ostream& output) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mosqueeze
```

### ZpaqEngine.cpp — Approccio Streaming

**NOTA**: ZPAQ nativo è file-based. Per streaming, usare libzpaq con adapter.

```cpp
#include <mosqueeze/engines/ZpaqEngine.hpp>

#include "platform/Platform.hpp"

#include <libzpaq.h>

#include <array>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <thread>

namespace mosqueeze {
namespace {

constexpr size_t BUFFER_SIZE = 64 * 1024;
constexpr int ZPAQ_DEFAULT_THREADS = 1; // ZPAQ thread count

// Adapter class per streaming ZPAQ
class StreamReader : public libzpaq::Reader {
public:
    explicit StreamReader(std::istream& is) : stream_(is), pos_(0) {}
    
    int get() override {
        if (stream_.eof()) return -1;
        int c = stream_.get();
        if (c == std::char_traits<char>::eof()) return -1;
        ++pos_;
        return c;
    }
    
    int read(char* buf, int n) override {
        stream_.read(buf, n);
        int got = static_cast<int>(stream_.gcount());
        pos_ += got;
        return got;
    }
    
    size_t tell() const { return pos_; }
    
private:
    std::istream& stream_;
    size_t pos_;
};

class StreamWriter : public libzpaq::Writer {
public:
    explicit StreamWriter(std::ostream& os) : stream_(os), pos_(0) {}
    
    void put(int c) override {
        stream_.put(static_cast<char>(c));
        ++pos_;
    }
    
    void write(const char* buf, int n) override {
        stream_.write(buf, n);
        pos_ += n;
    }
    
    size_t tell() const { return pos_; }
    
private:
    std::ostream& stream_;
    size_t pos_;
};

int levelToMethod(int level) {
    // ZPAQ methods: 1-5
    return std::clamp(level, 1, 5);
}

} // namespace

struct ZpaqEngine::Impl {
    // ZPAQ state se necessario
};

ZpaqEngine::ZpaqEngine() : impl_(std::make_unique<Impl>()) {}

ZpaqEngine::~ZpaqEngine() = default;

std::vector<int> ZpaqEngine::supportedLevels() const {
    return {1, 2, 3, 4, 5};
}

CompressionResult ZpaqEngine::compress(
    std::istream& input,
    std::ostream& output,
    const CompressionOptions& opts) {
    
    auto startedAt = std::chrono::steady_clock::now();
    
    int method = levelToMethod(opts.level > 0 ? opts.level : defaultLevel());
    
    StreamReader inReader(input);
    StreamWriter outWriter(output);
    
    // ZPAQ compression arguments
    libzpaq::Compressor comp;
    const char* argv[] = {
        "zpaq",
        "compress",
        nullptr,  // method
        nullptr,  // threads
    };
    
    // Method string: "1" to "5"
    std::string methodStr = std::to_string(method);
    char methodArg[16];
    std::snprintf(methodArg, sizeof(methodArg), "-m%s", methodStr.c_str());
    
    comp.setOutput(&outWriter);
    
    // Compess with method
    CompressionResult result{};
    try {
        comp.compress(&inReader, methodStr.c_str(), ZPAQ_DEFAULT_THREADS);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("ZPAQ compression failed: ") + e.what());
    }
    
    result.originalBytes = inReader.tell();
    result.compressedBytes = outWriter.tell();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startedAt);
    result.peakMemoryBytes = platform::getPeakMemoryUsage();
    
    return result;
}

void ZpaqEngine::decompress(
    std::istream& input,
    std::ostream& output) {
    
    StreamReader inReader(input);
    StreamWriter outWriter(output);
    
    try {
        libzpaq::decompress(&inReader, &outWriter);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("ZPAQ decompression failed: ") + e.what());
    }
}

} // namespace mosqueeze
```

---

## Parte 4: Vendoring ZPAQ

### Download Source

```bash
# Scaricare libzpaq
cd third_party
git clone --depth 1 https://github.com/zpaq/zpaq.git zpaq-src
```

### third_party/zpaq/CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)

# libzpaq as static library
add_library(libzpaq STATIC
    libzpaq/libzpaq.cpp
)

target_include_directories(libzpaq
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/libzpaq
)

# ZPAQ requires C++11 minimum but works with C++20
target_compile_features(libzpaq PUBLIC cxx_std_17)

# Define for public domain
target_compile_definitions(libzpaq PRIVATE NOPROTECT=1)

# Optional: build zpaq CLI tool
option(BUILD_ZPAQ_CLI "Build zpaq command line tool" OFF)
if(BUILD_ZPAQ_CLI)
    add_executable(zpaq-cli
        zpaq.cpp
    )
    target_link_libraries(zpaq-cli PRIVATE libzpaq)
endif()
```

### Aggiornare CMake Principale

`CMakeLists.txt` (root):

```cmake
# Add third_party
add_subdirectory(third_party/zpaq)

# Link to libmosqueeze
target_link_libraries(libmosqueeze
    PRIVATE
        libzpaq
)
```

---

## Parte 5: Copia Vendored Files

### Script Setup

```bash
# scripts/vendor-zpaq.sh
#!/bin/bash
set -e

ZPAQ_VERSION="7.15"
ZPAQ_URL="https://github.com/zpaq/zpaq/archive/refs/tags/v${ZPAQ_VERSION}.tar.gz"

mkdir -p third_party/zpaq/libzpaq

# Download
curl -L "$ZPAQ_URL" -o /tmp/zpaq.tar.gz
tar -xzf /tmp/zpaq.tar.gz -C /tmp

# Copy libzpaq files
cp /tmp/zpaq-${ZPAQ_VERSION}/libzpaq.cpp third_party/zpaq/libzpaq/
cp /tmp/zpaq-${ZPAQ_VERSION}/libzpaq.h third_party/zpaq/libzpaq/

# Copy license
cp /tmp/zpaq-${ZPAQ_VERSION}/COPYING third_party/zpaq/

echo "ZPAQ ${ZPAQ_VERSION} vendored successfully"
```

---

## Parte 6: Benchmark Comparison

### Expected Results

| File Type | zstd-22 | lzma-9 | zpaq-5 |
|-----------|---------|--------|--------|
| Text (JSON) | 13-15x | 11-12x | 16-20x |
| Source code | 10-12x | 9-11x | 12-15x |
| Binary (exe) | 2.2-2.5x | 2.8-3x | 3-4x |
| Database | 2.5-3x | 2.8-3.5x | 3.5-5x |
| Mixed archive | 3-4x | 3.5-4x | 5-7x |

### Encode Time Comparison

| Algorithm | Time (GB file) |
|-----------|----------------|
| zstd-22 | ~30 seconds |
| lzma-9 | ~3 minutes |
| zpaq-5 | **30-60 minutes** |

**Nota**: ZPAQ è 60-100x più lento di zstd, ma per cold storage è accettabile.

---

## Parte 7: Test

### tests/unit/ZpaqEngine_test.cpp

```cpp
#include <mosqueeze/engines/ZpaqEngine.hpp>

#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// Test with smaller data for CI
// ZPAQ is very slow, so use minimal data for unit tests

int main() {
    mosqueeze::ZpaqEngine engine;
    
    // Test 1: Minimal roundtrip
    std::string original = "Hello, ZPAQ! Testing with minimal data. ";
    original += original;
    original += original;
    original += original; // 512 bytes total
    
    std::istringstream input(original);
    std::ostringstream compressed;
    
    std::cout << "[INFO] Compressing " << original.size() << " bytes with ZPAQ...\n";
    std::cout << "[INFO] This may take a few seconds...\n";
    
    auto start = std::chrono::steady_clock::now();
    auto result = engine.compress(input, compressed);
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start);
    
    std::cout << "[INFO] Compression time: " << elapsed.count() << "s\n";
    std::cout << "[INFO] Original: " << result.originalBytes << " bytes\n";
    std::cout << "[INFO] Compressed: " << result.compressedBytes << " bytes\n";
    std::cout << "[INFO] Ratio: " << result.ratio() << "x\n";
    
    // ZPAQ should at least compress a bit
    assert(result.ratio() >= 1.0);
    
    // Test decompression
    std::istringstream compressedInput(compressed.str());
    std::ostringstream decompressed;
    
    engine.decompress(compressedInput, decompressed);
    assert(decompressed.str() == original);
    
    // Test metadata
    auto levels = engine.supportedLevels();
    assert(levels.size() == 5);
    assert(engine.name() == "zpaq");
    assert(engine.fileExtension() == ".zpaq");
    assert(engine.defaultLevel() == 5);
    assert(engine.maxLevel() == 5);
    
    std::cout << "[PASS] ZpaqEngine roundtrip OK\n";
    return 0;
}
```

### Large File Test (Separato)

```cpp
// tests/integration/ZpaqEngine_large_test.cpp
// Run separately: ctest -R LargeFile -L integration

// Test 10MB file
// Test 100MB file
// Verify ratio improvement over zstd/lzma
```

---

## Parte 8: Aggiornamenti Secondari

### Algorithm Selector

Aggiornare `AlgorithmSelector` per includere zpaq:

```cpp
// In typeToRule_ per unknown/file che richiedono max compression
typeToRule_[FileType::Binary_Database] = {
    FileType::Binary_Database,
    "zpaq", 5, "Database: ZPAQ level 5 for maximum cold storage ratio",
    false, "lzma", 9
};
```

### Wiki Update

Aggiornare `docs/wiki/algorithms/zpaq.md`:

```markdown
# ZPAQ

**Summary**: Algoritmo ad altissima compressione per cold storage estremo.

**Best for**: Database, log storici, backup a lungo termine.

**Ratio**: 4-6x su testo, 3-4x su binari.

**Encode time**: 30-60 minuti per GB (100x più lento di zstd).

**Use case**: Archivi che saranno letti migliaia di volte, scritti una sola.
```

---

## Vincoli e Note

1. **Lentissimo**: ZPAQ impiega minuti per MB. Solo per cold storage.
2. **Public Domain**: Licenza compatibile con qualunque progetto.
3. **Memory**: ZPAQ può usare molta RAM a method 5. Testare su file grandi.
4. **Streaming limitato**: libzpaq supporta streaming, ma alcune funzionalità richiedono file-based.
5. **Thread safety**: ZPAQ è single-threaded per compressione. Multi-thread solo per multi-file.
6. **Error handling**: ZPAQ lancia eccezioni C++. Wrappare con `std::runtime_error`.

---

## Build Verification

```bash
# Build
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build

# Test
cd build
ctest -R ZpaqEngine --output-on-failure

# Large file test (optional)
./tests/integration/ZpaqEngine_large_test
```

---

## Definizione di Fatto

- [ ] `third_party/zpaq/` con libzpaq source vendored
- [ ] `third_party/zpaq/CMakeLists.txt` configurato
- [ ] `ZpaqEngine.hpp/cpp` implementa ICompressionEngine
- [ ] Streaming adapter per libzpaq Reader/Writer
- [ ] `ZpaqEngine_test.cpp` passa roundtrip minimo
- [ ] Large file test (opzionale, CI skip)
- [ ] `libmosqueeze/CMakeLists.txt` linka `libzpaq`
- [ ] `vcpkg.json` non richiede dipendenze extra (vendor)
- [ ] Wiki aggiornato con `algorithms/zpaq.md`
- [ ] CI passa su Linux, macOS, Windows
- [ ] Ratio verificato > zstd su almeno un file tipo

---

## Riferimenti

- [ZPAQ GitHub](https://github.com/zpaq/zpaq)
- [libzpaq Documentation](https://github.com/zpaq/zpaq/blob/master/libzpaq.h)
- [ZPAQ Format Specification](http://mattmahoney.net/dc/zpaq.html)
- Matt Mahoney's Data Compression Resources