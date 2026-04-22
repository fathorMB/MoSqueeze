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

### Feature Docs: Video Support (Issue #21)
- `algorithms/video-compression.md` - AVI/MKV strategy for cold storage with ZPAQ level 5
- Updated `index.md` links to include video compression guidance

### Feature Docs: Enhanced Benchmark Tool (Issue #23)
- Added `guides/benchmark-tool.md` with full enhanced `mosqueeze-bench` CLI reference
- Updated `benchmarks/methodology.md` to match current implementation:
  - config-driven execution (`runWithConfig`)
  - iterations + warmup
  - timeout, decode/memory toggles
  - stats aggregation and export behavior
- Updated `guides/getting-started.md` benchmark examples for new options
- Updated `index.md` to link the new benchmark guide

### Feature Docs: Preprocessing Engine (Issue #31)
- Added `preprocessing.md` with preprocessing overview and trailing-header format
- Updated `index.md` to include preprocessing section

### Feature Docs: Parallel Benchmark Hardening + UX
- Updated `guides/benchmark-tool.md` with:
  - `--json` / `--csv` export switches
  - parallel memory tracking note (auto-disabled when `--threads > 1`)
  - Windows runtime DLL troubleshooting note (`pugixml.dll` and transitive dependencies)
- Updated `benchmarks/methodology.md` to reflect parallel memory behavior and export switches

### Feature Docs: RAW + Bayer Preprocessing (Issues #35 and #36)
- Updated `preprocessing.md` to include RAW/TIFF routing and `bayer-raw`
- Added implementation-status notes so documentation matches current staged behavior
- Updated `guides/analyze-command.md` with current `--preprocess` modes including `bayer-raw`

### Feature Docs: Benchmark Visualization (Issue #40)
- Added `guides/benchmark-visualization.md` for `mosqueeze-viz` usage and comparison workflow
- Updated wiki `index.md` guide list to include benchmark visualization
- Updated top-level `README.md` quick links with visualization guide

### Feature Docs: Benchmark Preprocessing (Issue #43)
- Updated `guides/benchmark-tool.md` with `--preprocess` usage and output fields
- Updated `benchmarks/methodology.md` JSON schema example with preprocess metrics
- Updated `preprocessing.md` with direct `mosqueeze-bench --preprocess` usage example

### Feature Docs: Benchmark Progress Feedback (Issue #45)
- Updated `guides/benchmark-tool.md` with live progress/ETA behavior and mode interaction (`default`, `--verbose`, `--quiet`)

### Bug Fix Docs: Bayer RAF Parsing (Issue #46)
- Updated `preprocessing.md` with `bayer-raw` RAF region-only transform behavior
- Documented safe fallback mode for unparseable/non-RAF raw payloads
- Documented metadata compatibility (`version 2` region mode + legacy `version 1`)
