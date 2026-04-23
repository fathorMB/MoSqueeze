# Using `mosqueeze analyze`

## Purpose
Use `mosqueeze analyze` to inspect a file and get a recommended algorithm/level before running compression.

## Command

```bash
mosqueeze analyze <file> [-v|--verbose] [-b|--benchmark] [--preprocess <mode>]
```

## Intelligent Suggestion Mode

For recommendation-oriented workflow, use:

```bash
mosqueeze suggest <file> [--goal min-size|fastest|balanced|min-memory|best-decompression] [--db <sqlite-file>] [--json]
```

This mode uses the new intelligent selector pipeline (`FileAnalyzer` + optional `BenchmarkDatabase` + heuristic fallback) and returns preprocessor + algorithm + level with confidence.

## Current Behavior

- Detects file class and MIME via `FileTypeDetector`
- Runs `AlgorithmSelector`
- Prints:
  - file path
  - MIME type
  - compressed yes/no
  - recommended action
- Accepts `--preprocess` hint values:
  - `auto`, `none`, `json-canonical`, `xml-canonical`, `image-meta-strip`, `png-optimizer`, `bayer-raw`, `zstd-dict`

If file should be skipped, output includes explicit reason (for example already-compressed media).

## Examples

```bash
mosqueeze analyze README.md
mosqueeze analyze dataset.json --verbose
mosqueeze analyze archive.zip --verbose
mosqueeze analyze image.nef --preprocess bayer-raw
```

## Output Interpretation

- `Action: SKIP` => do not recompress as-is
- `Algorithm: <name> level <n>` => primary recommendation
- `Fallback: <name> level <n>` => fallback if primary fails or is unsuitable

## Notes

- `--benchmark` currently prints a placeholder message and points to `mosqueeze-bench` for full benchmark execution.
- Recommendations are cold-storage oriented (maximize ratio, decode still practical).
