# Wiki Log

Append-only record of all wiki changes.

---

## 2026-04-22

### Created
- Wiki structure: `docs/wiki/` with subdirectories
- `index.md` â€” Homepage with conceptual map
- `log.md` â€” This changelog

### Algorithms Section
- `algorithms/zstd.md` â€” Zstandard deep dive (levels 1-22, use cases)
- `algorithms/lzma-xz.md` â€” LZMA/XZ deep dive (binary optimization)
- `algorithms/brotli.md` â€” Brotli deep dive (text/web optimization)
- `algorithms/comparison-matrix.md` â€” Comparative table with metrics

### Decisions Section
- `decisions/file-type-to-algorithm.md` â€” Complete FileType â†’ Engine mapping
- `decisions/streaming-architecture.md` â€” ADR: why 64KB streaming
- `decisions/compression-levels.md` â€” Level selection guide

### Benchmarks Section
- `benchmarks/methodology.md` â€” How benchmarks are executed
- `benchmarks/corpus-selection.md` â€” Test corpus criteria
- `benchmarks/graphs/ratio-by-algorithm.md` â€” Mermaid charts
- `benchmarks/results/index.md` â€” Results archive index

### Guides Section
- `guides/getting-started.md` â€” Build, test, contribute guide
- `guides/adding-new-engine.md` â€” Step-by-step new algorithm
- `guides/cold-storage-patterns.md` â€” Best practices for archival

### Purpose
Initial wiki population for Issue #16 â€” focus on algorithmic decision-making, file type recommendations, and benchmark visualization. All pages follow NewsCentral wiki pattern (Karpathy LLM Wiki style).
### Feature Docs: Algorithm Selection (Issue #11)
- `decisions/algorithm-selection-engine.md` - Rule flow, benchmark override path, JSON rules support
- `guides/analyze-command.md` - CLI usage for `mosqueeze analyze`
- Updated `index.md` links for new decision/guide pages

### Feature Docs: ZPAQ Engine (Issue #10)
- `algorithms/zpaq.md` - ZPAQ overview, trade-offs, and cold-storage usage guidance
- Updated `index.md` algorithm map and links to include ZPAQ
- Updated `algorithms/comparison-matrix.md` to mark ZPAQ as implemented (vendored + streaming adapters)
- Updated `decisions/file-type-to-algorithm.md` with `Binary_Database -> zpaq level 5` (+ fallback)
- Updated `guides/adding-new-engine.md` with real vendoring integration path used by ZPAQ
