# Benchmark Methodology

**Summary**: How MoSqueeze benchmark runs are executed with the enhanced `mosqueeze-bench` tool.

**Last updated**: 2026-04-22

---

## Overview

MoSqueeze uses `mosqueeze-bench` to generate comparable measurements across engines and levels.

Example:

```bash
mosqueeze-bench --directory ./corpus --iterations 5 --warmup 2 --output benchmarks/results
```

---

## Measured Metrics

| Metric | Unit | Description |
|--------|------|-------------|
| Ratio | factor | `originalBytes / compressedBytes` |
| Encode Time | ms | Compression duration |
| Decode Time | ms | Decompression duration (optional) |
| Peak Memory | bytes | Peak memory from engine result (optional) |

Derived values:

- Throughput: `originalBytes / encodeTime`
- Space savings: `(1 - 1/ratio) * 100`

---

## Execution Model

For each selected file:

1. Select engines (`--algorithms` or `--all-engines`)
2. Select levels (`--levels`, `--default-only`, or engine defaults)
3. Run warmup iterations (`--warmup`)
4. Run measured iterations (`--iterations`)
5. Record result rows per measured run

Runtime controls:

- `--max-time` for per-step timeout guard
- `--no-decode` to skip decode benchmark
- `--no-memory` to disable memory field capture
- `--verbose` for progress callback output

---

## Statistics

`BenchmarkRunner::computeStats(...)` aggregates results by `algorithm/level`:

- mean ratio + stddev ratio
- mean encode time + stddev encode time
- mean decode time + stddev decode time
- mean peak memory
- sample count (`iterations`)

---

## Result Storage and Exports

Default output directory: `benchmarks/results`

- SQLite database: `results.sqlite3`
- Default exports (if `--export` is not set):
  - `results.json`
  - `results.csv`
- Explicit exports:
  - `--format json`
  - `--format csv`
  - `--format markdown`
  - `--format html`

JSON export format is an array of rows:

```json
[
  {
    "algorithm": "zstd",
    "level": 19,
    "file": "corpus/text/code/large.cpp",
    "fileType": 1,
    "originalBytes": 2100000,
    "compressedBytes": 540000,
    "encodeMs": 1200,
    "decodeMs": 45,
    "peakMemoryBytes": 134000000,
    "ratio": 3.89
  }
]
```

---

## Comparison Runs

You can compare current results against previous exports:

```bash
mosqueeze-bench --directory ./corpus --compare ./benchmarks/results/previous.json
mosqueeze-bench --directory ./corpus --compare ./benchmarks/results/previous.csv --diff-only
```

Comparison grouping key: `file + algorithm + level`.

---

## Related Pages

- [[corpus-selection]] - Test corpus criteria
- [[results/index]] - Historical benchmark archive
- [[../guides/benchmark-tool]] - Full CLI guide
