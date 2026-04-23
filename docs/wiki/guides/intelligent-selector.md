# Intelligent Selector (`mosqueeze suggest`)

**Summary**: Use the intelligent selector to get a compression recommendation (preprocessor + algorithm + level) from file features and optional benchmark history.

**Last updated**: 2026-04-23

---

## Command

```bash
mosqueeze suggest <file> [--goal <goal>] [--db <sqlite-file>] [--json]
```

Goals:

- `min-size` (default)
- `fastest`
- `balanced`
- `min-memory`
- `best-decompression`

---

## What It Uses

The selector combines:

- file feature analysis (`FileAnalyzer`)
  - MIME/type detection
  - extension hints
  - entropy and repeat-pattern signals
  - RAW compression classification when available
- optional SQLite benchmark/learning data (`BenchmarkDatabase`)
- heuristic fallback rules (`IntelligentSelector`) when no DB match exists

---

## Examples

```bash
# Default: prioritize smallest output
mosqueeze suggest ./dataset/config.json

# Use benchmark DB for data-driven recommendation
mosqueeze suggest ./photo.raf --db ./benchmarks/results/intelligent.sqlite3

# Prioritize speed
mosqueeze suggest ./archive.bin --goal fastest

# Compact machine-readable output
mosqueeze suggest ./input.png --json
```

---

## Output Fields

- `preprocessor`
- `algorithm`
- `level`
- `expectedRatio`
- `expectedTimeMs`
- `confidence`
- `sampleCount`
- `explanation`

When alternatives are available, the CLI also prints secondary options for `fastest` and `balanced`.

---

## Notes

- If no benchmark DB is provided (or no matching rows exist), recommendations come from built-in heuristics.
- RAW files use compression awareness:
  - uncompressed RAW can recommend `bayer-raw`
  - already-compressed RAW usually recommends `none`
