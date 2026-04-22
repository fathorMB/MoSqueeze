# Benchmark Visualization (`mosqueeze-viz`)

**Summary**: Generate HTML dashboards, Markdown reports, or JSON summaries from benchmark result files.

**Last updated**: 2026-04-22

---

## Overview

`mosqueeze-viz` reads benchmark outputs (`.json`, `.csv`, `.sqlite3`) and produces:

- HTML dashboard (`--format html`)
- Markdown report (`--format markdown`)
- JSON summary (`--format json-summary`)

It also supports baseline comparison with regression/improvement highlighting.

---

## Basic Usage

```bash
# HTML dashboard from default benchmark output
./mosqueeze-viz -i benchmarks/results/results.json -o dashboard.html

# Markdown summary
./mosqueeze-viz -i benchmarks/results/results.csv -f markdown -o results.md

# Machine-readable summary
./mosqueeze-viz -i benchmarks/results/results.sqlite3 -f json-summary
```

---

## Comparison Mode

```bash
# Compare current results against baseline
./mosqueeze-viz -i current.json --compare baseline.json -o comparison.html

# Show only significant diffs (>= 8%)
./mosqueeze-viz -i current.json --compare baseline.json --diff-only --threshold 8
```

Comparison is grouped by `file + algorithm + level` and uses averaged values per group.

---

## CLI Options

```text
Input:
  -i, --input FILE          Input results file (.json/.csv/.sqlite3)

Output:
  -o, --output FILE         Output path
  -f, --format FORMAT       html | markdown | json-summary

Comparison:
      --compare FILE        Baseline results file
      --diff-only           Keep only significant deltas
      --threshold PCT       Significant delta threshold (default: 5.0)

Display:
      --top N               Limit output rows (0 = all)
      --sort-by COLUMN      ratio | encode | decode | memory
      --no-charts           Disable Plotly chart embedding (HTML)

Misc:
  -v, --verbose             Verbose logs
```

---

## Notes

- HTML mode embeds Plotly from CDN by default.
- Use `--no-charts` for offline/static HTML without external scripts.
- `json-summary` writes to stdout and ignores `--output`.

---

## Related Pages

- [[benchmark-tool]] - Generate source benchmark results
- [[../benchmarks/methodology]] - Measurement model and stored fields
