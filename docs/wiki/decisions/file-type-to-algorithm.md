# FileType -> Algorithm Mapping

**Summary**: Mappatura tra tipi di file e algoritmo raccomandato per cold storage.

**Last updated**: 2026-04-23

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

| FileType | Preprocessing | Algoritmo | Livello | Ratio | Motivazione |
|----------|---------------|-----------|---------|-------|-------------|
| Image_PNG | oxipng | zstd | 22 | **1.120x** | Best cold storage (+2.5% vs baseline) |
| Image_PNG | none | zstd | 19 | 1.091x | Fast compression (42ms) |
| Image_JPEG | - | SKIP | - | - | Già lossy compresso |
| Image_WebP | - | SKIP | - | - | Già compresso |
| Image_Raw | bayer-raw | zpaq | 5 | ~1.000x | Max ratio per RAW (minimo gain) |
| Video_* | - | SKIP | - | - | Codec già compressi |
| Audio_WAV | - | lzma | 9 | - | PCM non compresso |
| Audio_MP3 / Audio_FLAC | - | SKIP | - | - | Già compresso |

**PNG Findings** (benchmark 2026-04-23, 1,445 files):
- PNG è comprimibile (~9% con ZSTD/19, ~12% con oxipng + ZSTD/22)
- Oxipng preprocessing aggiunge ~2.5% compressione (lossless)
- ZSTD/19 = ZSTD/22 in ratio ma 4x più veloce
- **Raccomandazione**: oxipng + ZSTD/22 per cold storage


### JPEG Findings (benchmark 2026-04-23, 49 files):

- JPEG è **già lossy-compresso** → 60-70% file diventano più grandi
- **SKIP raccomandato** per tutti i casi eccetto entropia molto bassa
- Eccezione: Entropia < 7.9 → considerare ZPAQ/5 (+15% potenziale)

| Algorithm | Files Peggiorati | Avg Ratio |
|-----------|------------------|-----------|
| ZSTD/22 | 61% | 1.008x |
| XZ/9 | 67% | 1.007x |
| BROTLI/11 | 71% | 1.010x |
| ZPAQ/5 | 0% | 1.018x (sicuro ma lento) |


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
| Archive_ZIP | SKIP | Extract then recompress | Archive già compresso |
| Archive_TAR | zstd | 22 | TAR non compresso |
| Archive_7Z | SKIP | Extract then recompress | Già compresso (tipicamente LZMA) |

---

## Selection Flow

1. Se `recommendation == skip` -> SKIP.
2. Se `recommendation == extract-then-compress` -> non comprimere direttamente.
3. Altrimenti usare regola per `FileType`.
4. Se presente benchmark data, può sovrascrivere la regola statica.

---

## Benchmark Data

Risultati PNG (vedi [[../benchmarks/png-oxipng-results]]):

| Config | Ratio | Encode Time | Use Case |
|--------|-------|-------------|----------|
| ZSTD/19 (baseline) | 1.091x | 42ms | Fast compression |
| ZSTD/22 (baseline) | 1.091x | 180ms | Not recommended |
| **Oxipng + ZSTD/22** | **1.120x** | 370ms | **Cold storage** |
| BROTLI/1 (baseline) | 1.057x | ~0ms | Instant compression |

---

## Related Pages

- [[algorithm-selection-engine]]
- [[../algorithms/comparison-matrix]]
- [[../algorithms/zpaq]]
- [[../benchmarks/png-oxipng-results]] – PNG benchmark results
- [[../benchmarks/png-full-matrix-results]] – Full PNG level matrix
