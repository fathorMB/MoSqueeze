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
- progress callback in verbose mode (`--verbose`)
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

Output:
  -o, --output DIR
      --export FILE
      --format FORMAT      (json|csv|markdown|html)
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
- Optional manual export:
  - `--format markdown` for report-friendly tables
  - `--format html` for browser report + chart

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

## Implementation Notes

- `BenchmarkRunner::runWithConfig(...)` is the primary execution path.
- Legacy `run()` and `runGrid()` remain supported.
- Progress callbacks are rate-limited to avoid console flooding.
- `computeStats(...)` returns aggregated means and standard deviations grouped by `algorithm/level`.

---

## Related Pages

- [[../benchmarks/methodology]] - Measurement methodology
- [[getting-started]] - Build and run basics
- [[../benchmarks/results/index]] - Historical run index
