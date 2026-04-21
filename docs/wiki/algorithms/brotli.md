# Brotli

**Summary**: Algoritmo ottimizzato per testo web, eccellente per JSON/XML/HTML.

**Best for**: Text-heavy content, web assets, structured text (JSON/XML).

**Last updated**: 2026-04-22

---

## Panoramica

Brotli è sviluppato da Google, originariamente per compressione HTTP. Offre:
- **Miglior ratio per testo** — Spesso 10-20% meglio di zstd su HTML/JSON
- **Quality 0-11** — Scala diversa da zstd/lzma
- **Decode veloce** — Ottimizzato per browser

## Quality Levels

| Quality | Speed | Ratio | Use Case |
|---------|-------|-------|----------|
| 0-4 | Molto veloce | 2.0-2.5x | Preview, dev |
| 5-8 | Medio | 2.5-3.5x | Bilanciato |
| 9-10 | Lento | 3.5-4.0x | Cold storage |
| 11 | Molto lento | 4.0-4.5x | Cold storage estremo |

**Nota**: Quality 11 è il default per MoSqueeze cold storage.

## Quando Usare Brotli

### ✅ Ideale Per

1. **JSON** — API responses, dataset, config
2. **XML** — RSS, SOAP, documenti strutturati
3. **HTML** — Pagina statica, template
4. **CSS/JavaScript** — Web assets
5. **Markdown** — Documentazione, README

### ⚠️ Valutare Per

1. **Codice sorgente** — zstd spesso più veloce a ratio simile
2. **Log files** — zstd/lzma possono essere migliori

### ❌ Evitare Per

1. **Binary data** — Non ottimizzato per questo
2. **Images/Video** — Use formati specifici
3. **Encrypted data** — Incompressible

## Esempio Benchmark

```
File: corpus/text/json/large_dataset.json (5.2 MB)

| Algorithm | Level | Compressed | Ratio | Encode Time |
|-----------|-------|------------|-------|-------------|
| zstd      | 19    | 420 KB     | 12.4x | 0.8s        |
| zstd      | 22    | 380 KB     | 13.7x | 2.1s        |
| brotli    | 9     | 340 KB     | 15.3x | 3.2s        |
| brotli    | 11    | 310 KB     | 16.8x | 18s         |
```

**Nota**: Brotli batte zstd di ~15% su JSON.

## Formato .br

MoSqueeze usa il formato Brotli raw con estensione `.br`:
- Header: nessuno (raw brotli stream)
- Riconoscimento: Per estensione o `detectFromMagic` meno affidabile
- Portable: Supportato da nginx, apache, cloudflare

## Streaming Architecture

```cpp
BrotliEncoderState* encoder = BrotliEncoderCreateInstance(nullptr, nullptr, nullptr);
BrotliEncoderSetParameter(encoder, BROTLI_PARAM_QUALITY, quality);

while (input) {
    read 64KB chunk
    BrotliEncoderCompressStream(encoder, BROTLI_OPERATION_PROCESS, ...)
    while (BrotliEncoderHasMoreOutput(encoder)) {
        write output
    }
}
// Finalize
BrotliEncoderCompressStream(encoder, BROTLI_OPERATION_FINISH, ...)
```

**Memory**: Dipende da quality, tipicamente 1-4MB dictionary

## Related Pages

- [[zstd]] — Alternativa più veloce
- [[lzma-xz]] — Per binari
- [[decisions/file-type-to-algorithm]] — Mappatura completa
- [[benchmarks/graphs/ratio-by-algorithm]] — Confronto visuale