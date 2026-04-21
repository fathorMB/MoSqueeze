# Zstandard (zstd)

**Summary**: Algoritmo di compressione general-purpose, default per la maggior parte dei file.

**Best for**: Testo, dati strutturati, quando encode/decode speed matters.

**Last updated**: 2026-04-22

---

## Panoramica

Zstandard (zstd) è un algoritmo di compressione lossless sviluppato da Facebook/Meta. Offre un ottimo bilanciamento tra:
- **Compression ratio**: Competitivo con LZMA
- **Speed**: 2-10x più veloce di LZMA a ratio simili
- **Memory**: Più efficiente di zpaq

## Compression Levels

| Level | Speed | Ratio | Memoria | Use Case |
|-------|-------|-------|---------|----------|
| 1-3 | Molto veloce | 2.0-2.5x | ~32MB | Interattivo, streaming |
| 4-10 | Veloce | 2.5-3.5x | ~64MB | Default generale |
| 11-19 | Medio | 3.5-4.0x | ~128MB | Cold storage bilanciato |
| 20-22 | Lento | 4.0-4.5x | ~256MB | Cold storage estremo |

### Ultra Mode

Con `--ultra` flag, zstd può usare livelli fino a 22. Per cold storage, raccomandiamo:
- **Level 19**: Bilanciato (ratio ~3.8x, encode time accettabile)
- **Level 22 ultra**: Massimo ratio (~4.2x), encode time importante

## Quando Usare zstd

### ✅ Ideale Per

1. **Codice sorgente** — C++, Python, JavaScript, ecc.
2. **Dati strutturati** — JSON, XML, YAML, CSV
3. **Log files** — Quando decode speed è importante
4. **Archivi generici** — Tar, backup misti
5. **Streaming real-time** — Compressione on-the-fly

### ⚠️ Migliorabile Per

1. **Binary executables** — LZMA spesso migliore
2. **Database files** — LZMA gestisce meglio pattern ripetitivi
3. **Testo web puro** — Brotli può avere 10-15% ratio migliore

### ❌ Evitare Per

1. **File già compressi** — JPEG, PNG, MP4 (spreco CPU, ratio ~1.0)
2. **Encrypted data** — Incompressible per definizione

## Esempio Benchmark

```
File: corpus/text/code/cpp_large.cpp (2.1 MB)

| Algorithm | Level | Compressed | Ratio | Encode Time |
|-----------|-------|------------|-------|-------------|
| zstd      | 3     | 520 KB     | 4.0x  | 0.08s       |
| zstd      | 19    | 410 KB     | 5.1x  | 1.2s        |
| zstd      | 22    | 395 KB     | 5.3x  | 4.5s        |
```

## Streaming Architecture

zstd in MoSqueeze usa API streaming con 64KB buffer:

```cpp
ZSTD_CStream* cstream = ZSTD_createCStream();
ZSTD_initCStream(cstream, level);

while (input) {
    read 64KB chunk
    ZSTD_compressStream2(cstream, &outBuffer, &inBuffer, ZSTD_e_continue)
    write output
}
```

**Vantaggi**:
- Nessun limite dimensione file
- Memoria costante (~64KB buffer)
- Adatto per file multi-GB

## Related Pages

- [[lzma-xz]] — Alternativa per binari
- [[brotli]] — Alternativa per testo web
- [[decisions/file-type-to-algorithm]] — Mappatura completa
- [[decisions/streaming-architecture]] — ADR sul perché streaming