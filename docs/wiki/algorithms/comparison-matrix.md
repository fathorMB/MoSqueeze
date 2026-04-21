# Algorithm Comparison Matrix

**Summary**: Tabella comparativa completa degli algoritmi supportati.

**Last updated**: 2026-04-22

---

## Quick Reference

| | zstd | lzma/xz | brotli | zpaq |
|---|------|---------|--------|------|
| **Ratio (text)** | 3.5-4.2x | 3.0-3.8x | 4.0-4.5x | 5.0-7.0x |
| **Ratio (binary)** | 1.8-2.2x | 2.5-2.8x | 1.5-1.8x | 3.0-4.0x |
| **Encode Speed** | Fast | Medium | Medium/Slow | Very Slow |
| **Decode Speed** | Fast | Medium | Fast | Slow |
| **Memory** | Low | Medium | Low | High |
| **Streaming API** | Yes | Yes | Yes | Yes (Reader/Writer adapters) |
| **Levels** | 1-22 | 0-9 | 0-11 | 1-5 |

---

## Best Use Cases

### zstd

- Codice sorgente
- Dati strutturati generici
- Archivi misti
- Default quando non si conosce il contenuto

### lzma/xz

- Executable e binari strutturati
- Fallback robusto per cold storage
- Log con pattern ripetitivi

### brotli

- JSON/XML
- HTML/CSS/JS
- Markdown e testo naturale

### zpaq

- Database storici
- Backup cold storage con write-once/read-rarely
- Dataset dove il ratio massimo e piu importante del tempo di encode

---

## Practical Notes

- ZPAQ e integrato via `third_party/zpaq/libzpaq` (vendored), non via vcpkg.
- Nel selector corrente, i database (`Binary_Database`) preferiscono `zpaq level 5` con fallback `lzma level 9`.
- Per workload operativi con tempi stretti, preferire `zstd` o `lzma`; usare `zpaq` per archiviazione estrema.

---

## Related Pages

- [[zstd]]
- [[lzma-xz]]
- [[brotli]]
- [[zpaq]]
- [[decisions/file-type-to-algorithm]]
