# LZMA / XZ

**Summary**: Algoritmo ad alto ratio per binari, database e dati con pattern ripetitivi.

**Best for**: Binary executables, database files, log strutturati.

**Last updated**: 2026-04-22

---

## Panoramica

LZMA (Lempel-Ziv-Markov chain Algorithm) è la base del formato XZ. Offre:
- **Miglior ratio** per dati binari strutturati
- **Compressione lenta** — encode time significativo
- **Decode veloce** — più veloce di zpaq

## Compression Levels

| Level | Preset | Ratio | Encode Time | Use Case |
|-------|--------|-------|-------------|----------|
| 0 | --fast | 2.0x | Veloce | Preview rapido |
| 1-3 | Low | 2.5-3.0x | Medio | Bilanciato |
| 4-6 | Medium | 3.0-3.5x | Medio | Default |
| 7-8 | High | 3.5-4.0x | Lento | Cold storage |
| 9 | --extreme | 4.0-4.5x | Molto lento | Cold storage massimo |

### Extreme Mode

In MoSqueeze, `opts.extreme` abilita `LZMA_PRESET_EXTREME`:
- Migliora ratio del 2-5%
- Raddoppia/tripla encode time
- Ideale per cold storage dove time è irrilevante

## Quando Usare LZMA

### ✅ Ideale Per

1. **Binary executables** — ELF, PE/EXE, Mach-O
2. **Database files** — SQLite, MDB, file di dati
3. **Log strutturati** — Con timestamp, pattern ripetitivi
4. **Dati scientifici** — Array numerici, misurazioni
5. **Archive compressi** — Quando bisogna estrarre e ricomprimere

### ⚠️ Valutare Per

1. **Testo puro** — zstd/brotli spesso più veloce a ratio simile
2. **File piccoli** — Overhead header XZ (32 byte)

### ❌ Evitare Per

1. **Real-time streaming** — Troppo lento per compressione live
2. **Interattività** — UI freezing durante compressione
3. **File già compressi** — Come tutti gli algoritmi

## Esempio Benchmark

```
File: corpus/binary/executable (15 MB)

| Algorithm | Level | Compressed | Ratio | Encode Time |
|-----------|-------|------------|-------|-------------|
| zstd      | 19    | 8.2 MB     | 1.8x  | 1.1s        |
| zstd      | 22    | 7.8 MB     | 1.9x  | 3.4s        |
| lzma      | 6     | 6.1 MB     | 2.5x  | 8.2s        |
| lzma      | 9     | 5.4 MB     | 2.8x  | 45s         |
```

**Nota**: LZMA batte zstd di ~40% su questo tipo di file.

## Formato XZ

MoSqueeze usa il formato XZ (non LZMA raw):
- Header: `FD 37 7A 58 5A 00`
- Checksum: CRC64 (standard)
- Filtri: LZMA2 automatico

**Vantaggi XZ**:
- Standardizzato e portabile
- Check integrato per integrità
- Multi-stream support

## Streaming Architecture

```cpp
lzma_stream strm = LZMA_STREAM_INIT;
lzma_easy_encoder(&strm, level, LZMA_CHECK_CRC64);

while (input) {
    read 64KB chunk
    lzma_code(&strm, LZMA_RUN)
    write output
}
// Finalize
lzma_code(&strm, LZMA_FINISH)
```

**Memory**: ~64KB buffer + dictionary size (level-dependent)

## Related Pages

- [[zstd]] — Alternativa più veloce
- [[brotli]] — Per testo web
- [[decisions/file-type-to-algorithm]] — Mappatura completa
- [[benchmarks/graphs/ratio-by-algorithm]] — Confronto visuale