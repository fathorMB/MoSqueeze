# Benchmark Tool (`mosqueeze-bench`)

**Summary**: Practical guide for the enhanced benchmark CLI implemented in Issue #23.

**Last updated**: 2026-04-22

---

## Overview

`mosqueeze-bench` supports:

- arbitrary input selection (`--file`, `--directory`, `--glob`, `--stdin`)
- engine/level selection (`--algorithms`, `--levels`, `--all-engines`, `--default-only`)
- repeated runs with warmup (`--iterations`, `--warmup`)
- optional decode and memory tracking (`--no-decode`, `--no-memory`)
- optional preprocessing (`--preprocess none|auto|bayer-raw|image-meta-strip|json-canonical|xml-canonical`)
- live in-place progress feedback (enabled by default, suppressed by `--quiet`)
- result exports (`json`, `csv`, `markdown`, `html`)
- previous run comparison (`--compare`, `--diff-only`)

---

## Basic Usage

```bash
# Run on standard corpus fallback
./mosqueeze-bench

# Single file, all engines, verbose progress
./mosqueeze-bench --file ./data/sample.db --all-engines --verbose

# Recursive directory with repeatability controls
./mosqueeze-bench --directory ./datasets --iterations 5 --warmup 2

# Glob with selected algorithm and level
./mosqueeze-bench --glob "*.mkv" --algorithms zpaq --levels 5
```

---

## Input Modes

```bash
# Add multiple files
./mosqueeze-bench --file a.bin --file b.json

# Read file list from stdin (one path per line)
find ./data -name "*.json" | ./mosqueeze-bench --stdin --iterations 3
```

If no explicit files are provided, the tool uses `--corpus` (default `benchmarks/corpus`).

---

## CLI Options

```text
Input:
  -f, --file PATH
  -d, --directory PATH
  -g, --glob PATTERN
  -c, --corpus PATH
      --stdin

Algorithm:
  -a, --algorithms LIST
  -l, --levels LIST
      --all-engines
      --default-only

Benchmark:
  -i, --iterations N
  -w, --warmup N
      --track-memory
      --no-memory
      --max-time SECONDS
      --decode
      --no-decode
      --preprocess MODE   (none|auto|bayer-raw|image-meta-strip|json-canonical|xml-canonical)

Output:
  -o, --output DIR
      --export FILE
      --format FORMAT      (json|csv|markdown|html)
      --json                Export JSON to <output>/results.json
      --csv                 Export CSV to <output>/results.csv
  -v, --verbose
  -q, --quiet
      --summary
      --no-color

Comparison:
      --compare FILE
      --diff-only

Misc:
      --dry-run
      --list-engines
      --version
```

---

## Output and Storage

- SQLite store: `benchmarks/results/results.sqlite3` (or `--output`)
- Auto-export defaults (when `--export` is not provided):
  - `results.json`
  - `results.csv`
- Explicit legacy-style export switches are also accepted:
  - `--json`
  - `--csv`
- Optional manual export:
  - `--format markdown` for report-friendly tables
  - `--format html` for browser report + chart
- Exported rows include preprocessing fields:
  - `preprocess.type`
  - `preprocess.originalBytes`
  - `preprocess.processedBytes`
  - `preprocess.timeMs`
  - `preprocess.improvement`
  - `preprocess.applied`
  - `totalRatio`

---

## Preprocessing Benchmarking

```bash
# Auto-select best preprocessor by detected file type
./mosqueeze-bench --directory ./datasets --preprocess auto --default-only --summary

# Force Bayer preprocessing for RAW-heavy datasets
./mosqueeze-bench --directory ./raw --preprocess bayer-raw --algorithms zstd --default-only -v

# Baseline run without preprocessing for A/B comparison
./mosqueeze-bench --directory ./raw --preprocess none --output ./benchmarks/results/no-pre
```

---

## Comparison Workflow

```bash
# Compare against a previous JSON export
./mosqueeze-bench --directory ./data --compare ./benchmarks/results/old.json

# Show only changed entries
./mosqueeze-bench --directory ./data --compare ./benchmarks/results/old.csv --diff-only
```

Comparison keys are grouped by `file + algorithm + level`.

---

## Parallel Execution

For large benchmark sets, use built-in file-level parallelism:

```bash
# Auto-detect worker threads
./mosqueeze-bench --directory ./images --default-only

# Explicit worker count
./mosqueeze-bench --directory ./images --threads 8

# Force single-threaded mode
./mosqueeze-bench --directory ./images --sequential
```

Notes:

- parallel mode processes different files concurrently
- each file still runs engines/levels sequentially within the worker
- `--verbose` progress updates are mutex-protected and rate-limited
- memory tracking is automatically disabled in parallel runs (`--threads > 1`)
  because peak memory sampling is process-wide and cannot be attributed per file

---

## Progress Feedback

`mosqueeze-bench` now renders an in-place progress line during execution:

- progress bar and percentage
- completed files / total files
- current file name
- current algorithm/level when available
- ETA after at least one file completes

Behavior by mode:

- default: progress line enabled
- `--verbose`: adds iteration details to the progress line
- `--quiet`: disables progress output

---

## Implementation Notes

- `BenchmarkRunner::runWithConfig(...)` is the primary execution path.
- `BenchmarkRunner::runParallel(...)` is used when effective thread count is greater than 1.
- Legacy `run()` and `runGrid()` remain supported.
- Progress callbacks are rate-limited to avoid console flooding.
- `computeStats(...)` returns aggregated means and standard deviations grouped by `algorithm/level`.

---

## Troubleshooting

On Windows, if `mosqueeze-bench.exe` reports missing DLLs (for example `pugixml.dll`), rebuild the benchmark target so runtime dependencies are copied next to the executable:

```bash
cmake --build build --config Release --target mosqueeze-bench
```

---

## Related Pages

- [[../benchmarks/methodology]] - Measurement methodology
- [[getting-started]] - Build and run basics
- [[../benchmarks/results/index]] - Historical run index
