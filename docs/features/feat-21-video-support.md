# Worker Spec: Video Support ottimizzato (Issue #21)

## Overview

Ottimizzare il supporto per file video AVI e MKV per cold storage, usando ZPAQ per estrarre compressione aggiuntiva.

## Stato Attuale

Il sistema HA GIÀ:
- `FileType::Video_MP4`, `Video_MKV`, `Video_WebM` in `Types.hpp`
- Regole di skip in `AlgorithmSelector.cpp` (line 64-80)
- Estensioni `.avi`, `.mkv`, `.mp4`, `.mov` mappate in `FileTypeDetector.cpp`

**Problema attuale:**
- Tutti i video sono marcati `skip` — nessuna compressione
- `.avi` è mappato a `Video_MP4` (tipo errato)
- Per cold storage, possiamo usare ZPAQ per estrarre ~10-20% extra anche da video compressi

## Analisi Video per Cold Storage

| Container | Contenuto tipico | Skip attuale | ZPAQ level 5 | Ratio stimato |
|-----------|------------------|--------------|--------------|---------------|
| AVI (raw/DV) | Frame non compressi | ❌ Skip (errato!) | ✅ ZPAQ 5 | 3-10x |
| AVI (DivX/XviD) | Già compresso | ✅ Skip | ✅ ZPAQ 5 | 1.1-1.3x |
| MKV (H.264/H.265) | Altamente compresso | ✅ Skip | ✅ ZPAQ 5 | 1.05-1.15x |
| MKV (raw video) | Non compresso | ✅ Skip | ✅ ZPAQ 5 | 2-5x |

**Raccomandazione:** Per cold storage, ZPAQ level 5 SU TUTTI i video (anche già compressi) — è lento ma estrae ogni bit possibile.

---

## Parte 1: Nuovo FileType per AVI

### Modifiche a `Types.hpp`

Aggiungere `Video_AVI` come tipo distinto:

```cpp
// In enum FileType, dopo Video_WebM:
    Video_MP4,
    Video_MKV,
    Video_WebM,
    Video_AVI,    // ← NUOVO

// Audio...
```

### Modifiche a `FileTypeDetector.cpp`

Rimuovi `.avi` dalla mappatura `Video_MP4` e crea entry dedicata:

```cpp
// Line 237: cambiare da:
{".avi", makeClassification(FileType::Video_MP4, "video/x-msvideo", true, false, ".avi")},

// A:
{".avi", makeClassification(FileType::Video_AVI, "video/x-msvideo", false, true, ".avi")},
```

**Nota:** `isCompressed = false`, `canRecompress = true` perché AVI può essere raw.

---

## Parte 2: Aggiornare AlgorithmSelector

### Nuove Regole per Video

```cpp
// In AlgorithmSelector::initializeDefaultRules()

// AVI: ZPAQ level 5 — può essere raw o compresso, sempre comprimere
typeToRule_[FileType::Video_AVI] = {
    FileType::Video_AVI,
    "zpaq", 5, "AVI: ZPAQ level 5 for maximum ratio (handles raw and compressed)",
    false, "lzma", 9
};

// MKV: ZPAQ level 5 — container con stream compressi, ZPAQ estrae il possibile
typeToRule_[FileType::Video_MKV] = {
    FileType::Video_MKV,
    "zpaq", 5, "MKV: ZPAQ level 5 for cold storage (extracts additional ~10-15%)",
    false, "zstd", 22
};

// MP4/WebM rimangono skip (H.264/265/VP9 già molto compressi, ratio <5%)
// A meno che benchmark non mostri beneficio ZPAQ
typeToRule_[FileType::Video_MP4] = {
    FileType::Video_MP4,
    "", 0, "MP4: H.264/265 already max compressed (ratio <5% from ZPAQ)",
    true, "", 0
};

typeToRule_[FileType::Video_WebM] = {
    FileType::Video_WebM,
    "", 0, "WebM: VP9/AV1 max compressed (ratio <5% from ZPAQ)",
    true, "", 0
};
```

---

## Parte 3: Rilevamento Video Raw vs Compresso (Opzionale)

Per AVI che potrebbero essere raw DV o compressi DivX:

### Opzione A: Magic Bytes Detection

```cpp
// In FileTypeDetector::detectFromMagic()
// AVI RIFF header
if (buffer.size() >= 12) {
    if (buffer[0] == 'R' && buffer[1] == 'I' && buffer[2] == 'F' && buffer[3] == 'F' &&
        buffer[8] == 'A' && buffer[9] == 'V' && buffer[10] == 'I' && buffer[11] == ' ') {
        
        // AVI file — check if raw or compressed
        // Look for 'vids' chunk and codec fourcc
        bool isRaw = checkIfRawAVI(buffer);  // DVSD, DVS1, etc.
        
        return makeClassification(
            FileType::Video_AVI,
            "video/x-msvideo",
            isRaw ? false : true,   // isCompressed
            true,                   // canRecompress
            ".avi"
        );
    }
}
```

### Opzione B: Sempre ZPAQ (Semplice)

Non distinguere — ZPAQ adatta automaticamente la compressione:
- Se raw → ratio alto
- Se già compresso → ratio basso ma sempre positivo

**Raccomandazione:** Opzione B per ora. Opzione A aggiunta in futuro se necessario.

---

## Parte 4: Test

### tests/unit/VideoFileType_test.cpp

```cpp
#include <mosqueeze/FileTypeDetector.hpp>
#include <mosqueeze/AlgorithmSelector.hpp>

#include <cassert>
#include <iostream>

int main() {
    mosqueeze::FileTypeDetector detector;
    mosqueeze::AlgorithmSelector selector;
    
    // Test AVI detection
    auto aviClass = detector.detectFromExtension(".avi");
    assert(aviClass.type == mosqueeze::FileType::Video_AVI);
    assert(aviClass.canRecompress == true);
    
    auto aviSelection = selector.select(aviClass);
    assert(aviSelection.algorithm == "zpaq");
    assert(aviSelection.level == 5);
    assert(aviSelection.shouldSkip == false);
    std::cout << "[PASS] AVI → ZPAQ level 5\n";
    
    // Test MKV
    auto mkvClass = detector.detectFromExtension(".mkv");
    assert(mkvClass.type == mosqueeze::FileType::Video_MKV);
    
    auto mkvSelection = selector.select(mkvClass);
    assert(mkvSelection.algorithm == "zpaq");
    assert(mkvSelection.level == 5);
    std::cout << "[PASS] MKV → ZPAQ level 5\n";
    
    // Test MP4 (should still skip)
    auto mp4Class = detector.detectFromExtension(".mp4");
    assert(mp4Class.type == mosqueeze::FileType::Video_MP4);
    
    auto mp4Selection = selector.select(mp4Class);
    assert(mp4Selection.shouldSkip == true);
    std::cout << "[PASS] MP4 → skip\n";
    
    // Test WebM (should still skip)
    auto webmClass = detector.detectFromExtension(".webm");
    assert(webmClass.type == mosqueeze::FileType::Video_WebM);
    
    auto webmSelection = selector.select(webmClass);
    assert(webmSelection.shouldSkip == true);
    std::cout << "[PASS] WebM → skip\n";
    
    return 0;
}
```

### tests/integration/VideoCompression_test.cpp

```cpp
// Richiede file video reali
// Test ZPAQ compression on sample.avi and sample.mkv
// Verifica ratio > 1.0 (anche marginalmente)
```

---

## Parte 5: Documentazione

### docs/wiki/algorithms/video-compression.md

```markdown
# Video Compression for Cold Storage

## Overview

MoSqueeze supporta compressione video per cold storage usando ZPAQ level 5.

## File Types Supportati

| Estensione | Tipo | Strategia | Ratio tipico |
|------------|------|-----------|--------------|
| `.avi` | Video_AVI | ZPAQ 5 | 1.1-10x |
| `.mkv` | Video_MKV | ZPAQ 5 | 1.05-1.15x |
| `.mp4` | Video_MP4 | Skip | — |
| `.webm` | Video_WebM | Skip | — |
| `.mov` | Video_MP4 | Skip | — |

## Perché ZPAQ per video?

I video sono già compressi con codec come H.264, H.265, VP9. Tuttavia:

1. **Container overhead**: MKV/AVI hanno struttura che può essere compressa
2. **Context mixing**: ZPAQ trova correlazioni tra frame che i codec intra-frame ignirano
3. **Metadata**: Titoli, capitoli, subtitle tracks comprimono bene
4. **Cold storage**: Encode time irrilevante, ogni bit conta

## Ratio Attesi

### AVI Raw/DV
- **Input**: Video DV non compresso
- **ZPAQ level 5**: 3-10x ratio
- **Confronto**: ZSTD level 22 ≈ 2-4x

### AVI DivX/XviD
- **Input**: Video già compresso
- **ZPAQ level 5**: 1.1-1.3x ratio
- **Nota**: Piccolo ma gratuito in cold storage

### MKV H.264/H.265
- **Input**: Container con stream compressi
- **ZPAQ level 5**: 1.05-1.15x ratio
- **Nota**: Principalmente container overhead + metadata

## Quando NON comprimere

- File in hot storage (accesso frequente)
- Streaming video (decode time critico)
- Formati già massimamente compressi (MP4 H.265)

## Alternativa: Estrazione Stream

Per massima compressione, estrarre stream video raw e comprimere separatamente:

```bash
# Estrarre stream video
ffmpeg -i input.mkv -c:v copy -f rawvideo video.raw

# Comprimere con ZPAQ
zpaq a video.raw.zpaq video.raw -method 5

# Ratio: 10-20x su video raw
```

**Nota**: Questo è fuori scope di MoSqueeze per ora.
```

---

## Parte 6: Aggiornamenti Secondari

### Magic Bytes per AVI

In `FileTypeDetector::detectFromMagic()`, aggiungere:

```cpp
// AVI/RIFF detection (lines ~320-350 in FileTypeDetector.cpp)
// RIFF....AVI 
if (buffer.size() >= 12 &&
    buffer[0] == 'R' && buffer[1] == 'I' && buffer[2] == 'F' && buffer[3] == 'F' &&
    buffer[8] == 'A' && buffer[9] == 'V' && buffer[10] == 'I') {
    return makeClassification(FileType::Video_AVI, "video/x-msvideo", false, true, ".avi");
}
```

---

## File da Modificare

### Obbligatori

1. `src/libmosqueeze/include/mosqueeze/Types.hpp`
   - Aggiungere `Video_AVI` all'enum

2. `src/libmosqueeze/src/FileTypeDetector.cpp`
   - Aggiungere magic bytes per AVI
   - Cambiare mappatura `.avi` da `Video_MP4` a `Video_AVI`
   - Impostare `isCompressed = false`, `canRecompress = true`

3. `src/libmosqueeze/src/AlgorithmSelector.cpp`
   - Aggiornare regola per `Video_AVI` → ZPAQ 5
   - Cambiare regola per `Video_MKV` → ZPAQ 5

### Test

4. `tests/unit/VideoFileType_test.cpp` (NUOVO)
   - Test detection e selection per AVI/MKV/MP4

### Documentazione

5. `docs/wiki/algorithms/video-compression.md` (NUOVO)

---

## Definizione di Fatto

- [ ] `Video_AVI` aggiunto a `Types.hpp`
- [ ] AVI rilevato correttamente per estensione
- [ ] AVI rilevato correttamente per magic bytes (RIFF header)
- [ ] `Video_AVI` → ZPAQ 5 in AlgorithmSelector
- [ ] `Video_MKV` → ZPAQ 5 in AlgorithmSelector
- [ ] Test unitari passano
- [ ] Wiki aggiornato con guidance video
- [ ] CI passa su Linux, macOS, Windows

---

## Note Tecniche

- **ZPAQ è lento**: Encode video può richiedere ore per GB. Accettabile per cold storage.
- **Ratio modesto su video compressi**: Aspettarsi 5-15% su MKV/MP4, ma è "gratis".
- **Fallback**: Se ZPAQ non disponibile, fallback a LZMA o ZSTD.
- **Non cambiare MP4/WebM skip**: H.265/VP9 hanno ratio <5% con ZPAQ, non vale il tempo CPU.

---

## Collegamenti

- Issue #10 (ZPAQ Engine) — prerequisito
- Issue #1 (File Type Detection)
- Issue #2 (Benchmark Harness)