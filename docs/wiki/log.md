# Wiki Log

Append-only record of all wiki changes.

---

## 2026-04-22

### Created
- Wiki structure: `docs/wiki/` with subdirectories
- `index.md` — Homepage with conceptual map
- `log.md` — This changelog

### Algorithms Section
- `algorithms/zstd.md` — Zstandard deep dive (levels 1-22, use cases)
- `algorithms/lzma-xz.md` — LZMA/XZ deep dive (binary optimization)
- `algorithms/brotli.md` — Brotli deep dive (text/web optimization)
- `algorithms/comparison-matrix.md` — Comparative table with metrics

### Decisions Section
- `decisions/file-type-to-algorithm.md` — Complete FileType → Engine mapping
- `decisions/streaming-architecture.md` — ADR: why 64KB streaming
- `decisions/compression-levels.md` — Level selection guide

### Benchmarks Section
- `benchmarks/methodology.md` — How benchmarks are executed
- `benchmarks/corpus-selection.md` — Test corpus criteria
- `benchmarks/graphs/ratio-by-algorithm.md` — Mermaid charts
- `benchmarks/results/index.md` — Results archive index

### Guides Section
- `guides/getting-started.md` — Build, test, contribute guide
- `guides/adding-new-engine.md` — Step-by-step new algorithm
- `guides/cold-storage-patterns.md` — Best practices for archival

### Purpose
Initial wiki population for Issue #16 — focus on algorithmic decision-making, file type recommendations, and benchmark visualization. All pages follow NewsCentral wiki pattern (Karpathy LLM Wiki style).