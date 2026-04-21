# Worker Spec: Compression Core - Zstd Engine + File Type Detection (Issues #7 + #8)

## Overview

Implementare i motori di compressione (a partire da Zstd) e il rilevamento tipo file (magic bytes + estensione + content sniffing) in un'unica botta. Questo è il cuore tecnico della libreria.

## Stato Attuale

Lo scaffold esiste già dalla issue #6. Il worker troverà:

- `src/libmosqueeze/include/mosqueeze/ICompressionEngine.hpp` — interfaccia già definita (metodi virtuali `compress`, `decompress`, metadata)
- `src/libmosqueeze/include/mosqueeze/Types.hpp` — `CompressionOptions`, `CompressionResult`, `FileType` enum già definiti
- `src/libmosqueeze/include/mosqueeze/FileTypeDetector.hpp` — stub vuoto (solo dichiarazione statica)
- `src/libmosqueeze/include/mosqueeze/AlgorithmSelector.hpp` — stub vuoto
- `src/libmosqueeze/CMakeLists.txt` — include solo `Version.cpp` + `Platform.cpp`
- Dipendenze vcpkg: `zstd`, `lzma`, `brotli`, `fmt`, `spdlog` installabili
- CLI11, fmt, spdlog linkati solo in mosqueeze-cli

## Obiettivo

Implementare entrambe le feature in modo che:
1. `libmosqueeze` compili con tutti i motori e il detector
2. I test di roundtrip per zstd passino
3. Il detector identifichi correttamente almeno 15 tipi file comuni
4. Il CI continui a passare (Linux, macOS, Windows)

---

## Parte 1: Zstd Compression Engine (Issue #7)

### File da creare

```
src/libmosqueeze/include/mosqueeze/engines/
└── ZstdEngine.hpp

src/libmosqueeze/src/engines/
└── ZstdEngine.cpp
```

### Requisiti Implementativi

#### ZstdEngine.hpp

```cpp
#pragma once
#include <mosqueeze/ICompressionEngine.hpp>
#include <vector>

namespace mosqueeze {

class ZstdEngine : public ICompressionEngine {
public:
    std::string name() const override { return "zstd"; }
    std::string fileExtension() const override { return ".zst"; }
    std::vector<int> supportedLevels() const override;
    int defaultLevel() const override { return 22; }
    int maxLevel() const override { return 22; }

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

#### ZstdEngine.cpp — Requisiti Streaming

**CRITICO: Deve usare API streaming di zstd, MAI caricare l'intero file in memoria.**

```cpp
// Pseudocodice compressione streaming
static constexpr size_t BUFFER_SIZE = 64 * 1024; // 64KB chunks

CompressionResult ZstdEngine::compress(...) {
    // Inizializzare ZSTD_CStream
    // Loop: leggere chunk da input (64KB) → comprimere con ZSTD → scrivere su output
    // Tracciare:
    //   - originalBytes (somma input)
    //   - compressedBytes (somma output)
    //   - duration (std::chrono)
    //   - peakMemoryBytes (usa platform::getPeakMemoryUsage se disponibile, altrimenti stima)
    // Livello: opts.level se specificato, altrimenti defaultLevel()
    // Se opts.extreme: usare ZSTD_maxCLevel() oppure livello 22
}

void ZstdEngine::decompress(...) {
    // Inizializzare ZSTD_DStream
    // Loop: leggere chunk compresso → decomprimere con ZSTD → scrivere su output
}
```

**Link CMake:** aggiungere `zstd` al `target_link_libraries` di `libmosqueeze` nel file `src/libmosqueeze/CMakeLists.txt`.

Nota: con vcpkg, il target CMake è tipicamente `zstd::libzstd_shared` o `zstd::libzstd_static`. Se non funziona, provare `zstd`. L'header è `<zstd.h>`.

---

## Parte 2: File Type Detection (Issue #8)

### File da creare/modificare

```
src/libmosqueeze/include/mosqueeze/FileTypeDetector.hpp     (già esiste, espandere)
src/libmosqueeze/src/FileTypeDetector.cpp                   (nuovo)
src/libmosqueeze/include/mosqueeze/FileClassification.hpp     (nuovo)
```

### Requisiti Implementativi

#### FileClassification.hpp

```cpp
#pragma once
#include <mosqueeze/Types.hpp>
#include <string>

namespace mosqueeze {

struct FileClassification {
    FileType type = FileType::Unknown;
    std::string mimeType = "application/octet-stream";
    std::string extension;
    bool isCompressed = false;      // Formato già compresso (PNG, JPEG, ZIP, MP4...)
    bool canRecompress = true;      // Vale la pena provare a comprimerlo ancora?
    std::string recommendation;     // "skip", "compress", "extract-then-compress"
};

} // namespace mosqueeze
```

#### FileTypeDetector.hpp

Sostituire lo stub con:

```cpp
#pragma once
#include <mosqueeze/FileClassification.hpp>
#include <filesystem>
#include <string>
#include <optional>

namespace mosqueeze {

class FileTypeDetector {
public:
    FileTypeDetector();

    FileClassification detect(const std::filesystem::path& path);

    // Rilevamento via magic bytes (più affidabile)
    FileClassification detectFromMagic(const std::vector<uint8_t>& buffer);

    // Rilevamento via estensione (fallback)
    FileClassification detectFromExtension(const std::string& ext);

    // Sniffing per file di testo (UTF-8, codice sorgente)
    FileClassification detectTextContent(const std::vector<uint8_t>& buffer);

private:
    // Mappa magic bytes → FileType
    struct MagicEntry {
        std::vector<uint8_t> bytes;
        size_t offset = 0;
        FileType type;
        std::string mime;
        bool isCompressed;
        bool canRecompress;
    };
    std::vector<MagicEntry> magicEntries_;

    void registerCommonTypes();
};

} // namespace mosqueeze
```

#### Magic Bytes da Supportare (Almeno Questi)

| Tipo | Magic Bytes (hex) | Offset | isCompressed | canRecompress |
|------|------------------|--------|-------------|---------------|
| PNG | `89 50 4E 47 0D 0A 1A 0A` | 0 | true | true (oxipng) |
| JPEG | `FF D8 FF` | 0 | true | false |
| GIF | `47 49 46 38` | 0 | true | true |
| BMP | `42 4D` | 0 | false | true |
| WebP | `52 49 46 46` → `57 45 42 50` @ offset 8 | 0 | true | false |
| PDF | `25 50 44 46` | 0 | true | true |
| ZIP | `50 4B 03 04` / `50 4B 05 06` | 0 | true | false |
| GZIP | `1F 8B` | 0 | true | false |
| ZSTD | `28 B5 2F FD` | 0 | true | false |
| XZ | `FD 37 7A 58 5A 00` | 0 | true | true |
| 7Z | `37 7A BC AF 27 1C` | 0 | true | false |
| MP4 | isobox signature (skip, troppo complesso) | - | true | false |
| ELF | `7F 45 4C 46` | 0 | false | true |
| PE/EXE | `4D 5A` | 0 | false | true |
| SQLite | `53 51 4C 69 74 65 20 66 6F 72 6D 61 74 20 33 00` | 0 | false | true |

#### Fallback per Estensione (Quando Magic Fails)

| Estensione | FileType | isCompressed | canRecompress |
|-----------|----------|-------------|---------------|
| `.txt`, `.md`, `.rst` | Text_Prose | false | true |
| `.c`, `.cpp`, `.h`, `.hpp`, `.cs`, `.py`, `.js`, `.ts`, `.rs`, `.go`, `.java` | Text_SourceCode | false | true |
| `.json`, `.xml`, `.yaml`, `.toml`, `.csv` | Text_Structured | false | true |
| `.log` | Text_Log | false | true |
| `.sql` | Text_Structured | false | true |
| `.html`, `.htm` | Text_Structured | false | true |
| `.css`, `.scss`, `.less` | Text_SourceCode | false | true |
| `.wav` | Audio_WAV | false | true |
| `.mp3` | Audio_MP3 | true | false |
| `.flac` | Audio_FLAC | true | false |
| `.aac`, `.ogg`, `.m4a` | Audio_MP3 | true | false |
| `.mp4`, `.mkv`, `.avi`, `.mov` | Video_MP4 | true | false |
| `.tar` | Archive_TAR | true | false |
| `.rar` | Archive_ZIP | true | false |
| `.db`, `.mdb` | Binary_Database | false | true |
| `.bin`, `.dat` | Binary_Chunked | false | true |
| `.wasm` | Binary_Executable | false | true |

#### Text Sniffing

Se il magic bytes non matcha nulla, leggere i primi 8KB del file:

```cpp
// Pseudocodice
bool isValidUtf8(const std::vector<uint8_t>& buffer);

// Se tutto il buffer è UTF-8 valido:
//   Controllare se contiene caratteri non-printable (control codes)
//   Se >95% printable → è testo
//   Identificare sottotipo:
//     - Inizia con "{" o "[" → JSON → Text_Structured
//     - Inizia con "<" → XML/HTML → Text_Structured
//     - Contiene molti ";", "{", "}" → codice → Text_SourceCode
//     - Data/ora all'inizio, timestamp → Log → Text_Log
//     - Altrimenti → Text_Prose
```

---

## Parte 3: Aggiornamenti CMake

### src/libmosqueeze/CMakeLists.txt

Deve includere i nuovi file sorgente:

```cmake
add_library(libmosqueeze SHARED
  src/Version.cpp
  src/platform/Platform.cpp
  src/engines/ZstdEngine.cpp          # NUOVO
  src/FileTypeDetector.cpp             # NUOVO
)

# Linkare zstd
target_link_libraries(libmosqueeze
  PRIVATE
    zstd::libzstd_shared   # oppure zstd — provare entrambi se necessario
)
```

Nota: se `zstd::libzstd_shared` non viene trovato da CMake, provare:
- `find_package(zstd CONFIG REQUIRED)` nel root CMakeLists.txt
- Oppure linkare direttamente con `zstd` (senza namespace)
- Se ancora fallisce, usare `find_library` e `find_path` manualmente

---

## Parte 4: Test

### Test Zstd (tests/unit/ZstdEngine_test.cpp)

```cpp
#include <mosqueeze/engines/ZstdEngine.hpp>
#include <sstream>
#include <cassert>
#include <iostream>

int main() {
    mosqueeze::ZstdEngine engine;
    
    // Test 1: compressione-decompressione roundtrip
    std::string original = "Hello, this is a test string for compression. ";
    original += original; // duplicare per avere pattern
    original += original;
    
    std::istringstream input(original);
    std::ostringstream compressed;
    
    auto result = engine.compress(input, compressed);
    assert(result.ratio() > 1.0);
    assert(result.compressedBytes < result.originalBytes);
    
    std::istringstream comprInput(compressed.str());
    std::ostringstream decompressed;
    engine.decompress(comprInput, decompressed);
    
    assert(decompressed.str() == original);
    
    // Test 2: livelli supportati
    auto levels = engine.supportedLevels();
    assert(!levels.empty());
    
    // Test 3: metadati
    assert(engine.name() == "zstd");
    assert(engine.fileExtension() == ".zst");
    assert(engine.maxLevel() >= engine.defaultLevel());
    
    std::cout << "[PASS] ZstdEngine roundtrip OK\n";
    return 0;
}
```

Aggiungere al `tests/CMakeLists.txt`:

```cmake
add_executable(ZstdEngine_test
  unit/ZstdEngine_test.cpp
)

target_link_libraries(ZstdEngine_test
  PRIVATE
    libmosqueeze
)

add_test(NAME ZstdEngine_test COMMAND ZstdEngine_test)
```

### Test FileTypeDetector (tests/unit/FileTypeDetector_test.cpp)

```cpp
#include <mosqueeze/FileTypeDetector.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cassert>

void writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(data.data()), data.size());
}

int main() {
    std::filesystem::create_directory("test_files");
    mosqueeze::FileTypeDetector detector;
    
    // Test PNG (magic bytes)
    std::vector<uint8_t> pngData = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    pngData.resize(100, 0);
    writeFile("test_files/test.png", pngData);
    auto cls = detector.detect("test_files/test.png");
    assert(cls.type == mosqueeze::FileType::Image_PNG);
    assert(cls.isCompressed);
    std::cout << "[PASS] PNG detection OK\n";
    
    // Test JPEG (magic bytes)
    std::vector<uint8_t> jpgData = {0xFF, 0xD8, 0xFF};
    jpgData.resize(100, 0);
    writeFile("test_files/test.jpg", jpgData);
    cls = detector.detect("test_files/test.jpg");
    assert(cls.type == mosqueeze::FileType::Image_JPEG);
    assert(!cls.canRecompress);
    std::cout << "[PASS] JPEG detection OK\n";
    
    // Test JSON (estensione)
    writeFile("test_files/data.json", {0x7B, 0x0A}); // {"\n"
    cls = detector.detect("test_files/data.json");
    assert(cls.type == mosqueeze::FileType::Text_Structured);
    std::cout << "[PASS] JSON detection OK\n";
    
    // Test MP4 (skip, isCompressed)
    writeFile("test_files/video.mp4", {0x00, 0x00, 0x00, 0x18, 0x66, 0x74, 0x79, 0x70});
    cls = detector.detect("test_files/video.mp4");
    assert(cls.isCompressed);
    assert(!cls.canRecompress);
    std::cout << "[PASS] MP4 detection OK\n";
    
    // Test unknown text (content sniffing)
    std::string text = "Hello world, this is plain text content.";
    std::vector<uint8_t> textData(text.begin(), text.end());
    writeFile("test_files/plain.txt", textData);
    cls = detector.detect("test_files/plain.txt");
    assert(cls.type == mosqueeze::FileType::Text_Prose);
    std::cout << "[PASS] Plain text detection OK\n";
    
    std::filesystem::remove_all("test_files");
    return 0;
}
```

Aggiungere al `tests/CMakeLists.txt`:

```cmake
add_executable(FileTypeDetector_test
  unit/FileTypeDetector_test.cpp
)

target_link_libraries(FileTypeDetector_test
  PRIVATE
    libmosqueeze
)

add_test(NAME FileTypeDetector_test COMMAND FileTypeDetector_test)
```

---

## Vincoli e Note

1. **Streaming solo**: Niente buffer dell'intero file in memoria. Usare sempre 64KB chunks per compressione/decompressione zstd.
2. **UTF-8 sniffing**: Implementare `isValidUtf8()` — non usare librerie esterne per questo, è semplice (state machine a 4 stati).
3. **Cross-platform**: Il codice deve compilare su Windows (MSVC), Linux (GCC), macOS (Clang) senza `#ifdef` inutili.
4. **Magic bytes**: Leggere i primi 16-32 byte di un file, fare match longest-prefix sulla tabella magic bytes.
5. **Estensione**: Estrarre estensione via `std::filesystem::path::extension()`, lowercase prima del confronto.
6. **Se vcpkg non trova zstd con namespace**: Provare `find_package(zstd CONFIG REQUIRED)` nel root `CMakeLists.txt`, altrimenti linkare target senza namespace.
7. **Error handling**: Se zstd restituisce errore (ZSTD_CStreamOutSize troppo piccolo, ecc.), lanciare `std::runtime_error` con messaggio descrittivo.

---

## Definizione di Fatto

- [ ] `ZstdEngine.cpp` implementa compressione/decompressione streaming zstd
- [ ] `ZstdEngine_test.cpp` passa (roundtrip, metadati corretti)
- [ ] `FileTypeDetector.cpp` rileva magic bytes per almeno 15 formati
- [ ] `FileTypeDetector.cpp` rileva estensione per almeno 20 tipi
- [ ] `FileTypeDetector.cpp` sniffing testo funziona per UTF-8 plain text, JSON, codice
- [ ] `FileTypeDetector_test.cpp` passa
- [ ] `libmosqueeze/CMakeLists.txt` aggiornato con i nuovi sorgenti
- [ ] CI passa su Linux, macOS, Windows
- [ ] Nessun warning di compilazione con `-Wall -Wextra`
