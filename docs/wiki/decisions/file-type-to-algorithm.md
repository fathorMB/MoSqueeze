# FileType -> Algorithm Mapping

**Summary**: Mappatura tra tipi di file e algoritmo raccomandato per cold storage.

**Last updated**: 2026-04-22

---

## Decision Matrix

### Text Files

| FileType | Algoritmo | Livello | Motivazione |
|----------|-----------|---------|-------------|
| Text_SourceCode | zstd | 22 | Alto ratio + decode veloce |
| Text_Structured | brotli | 11 | Ottimo su JSON/XML |
| Text_Prose | brotli | 11 | Ottimo su linguaggio naturale |
| Text_Log | lzma | 9 | Pattern ripetitivi |

### Image / Media Files

| FileType | Algoritmo | Livello | Motivazione |
|----------|-----------|---------|-------------|
| Image_PNG | zstd | 19 | Migliora spesso su zlib |
| Image_JPEG | SKIP | - | Gia lossy compresso |
| Image_WebP | SKIP | - | Gia compresso |
| Image_Raw | lzma | 9 | Pixel data non compresso |
| Video_* | SKIP | - | Codec gia compressi |
| Audio_WAV | lzma | 9 | PCM non compresso |
| Audio_MP3 / Audio_FLAC | SKIP | - | Gia compresso |

### Binary Files

| FileType | Algoritmo | Livello | Motivazione |
|----------|-----------|---------|-------------|
| Binary_Executable | lzma | 9 | Binario strutturato |
| Binary_Database | zpaq | 5 | Massimo ratio cold storage |
| Binary_Chunked | zstd | 22 | Default binario sicuro |

Fallback per `Binary_Database`: `lzma level 9`.

### Archives

| FileType | Algoritmo | Azione | Motivazione |
|----------|-----------|--------|-------------|
| Archive_ZIP | SKIP | Extract then recompress | Archive gia compresso |
| Archive_TAR | zstd | 22 | TAR non compresso |
| Archive_7Z | SKIP | Extract then recompress | Gia compresso (tipicamente LZMA) |

---

## Selection Flow

1. Se `recommendation == skip` -> SKIP.
2. Se `recommendation == extract-then-compress` -> non comprimere direttamente.
3. Altrimenti usare regola per `FileType`.
4. Se presente benchmark data, puo sovrascrivere la regola statica.

---

## Related Pages

- [[algorithm-selection-engine]]
- [[../algorithms/comparison-matrix]]
- [[../algorithms/zpaq]]
