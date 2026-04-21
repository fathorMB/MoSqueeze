# Algorithm Selection Engine

## Summary
`AlgorithmSelector` decides the best compression algorithm/level for a file based on:

1. File classification (`FileTypeDetector` + `FileClassification`)
2. Rule-based cold-storage defaults
3. Optional benchmark-driven overrides
4. Safety gates (`skip` / `extract-then-compress`)

## Implementation

- Header: `src/libmosqueeze/include/mosqueeze/AlgorithmSelector.hpp`
- Source: `src/libmosqueeze/src/AlgorithmSelector.cpp`
- Output model: `Selection`
  - `algorithm`
  - `level`
  - `shouldSkip`
  - `rationale`
  - `fallbackAlgorithm`
  - `fallbackLevel`

## Selection Flow

1. If `canRecompress == false` or `recommendation == "skip"` => skip.
2. If `recommendation == "extract-then-compress"` => skip and explain extraction-first flow.
3. If benchmark data is loaded => choose best ratio for the same `FileType`.
4. Else => apply default rule matrix.
5. Validate levels against registered engines (if available).

## Rule Matrix (Default)

- `Text_SourceCode` => `zstd:22` (fallback `brotli:11`)
- `Text_Structured` => `brotli:11` (fallback `zstd:22`)
- `Text_Prose` => `brotli:11` (fallback `zstd:22`)
- `Text_Log` => `lzma:9` (fallback `zstd:22`)
- `Image_PNG` => `zstd:19`
- `Image_JPEG`, `Image_WebP`, `Video_*`, `Audio_MP3`, `Audio_FLAC` => skip
- `Image_Raw`, `Audio_WAV`, `Binary_Executable`, `Binary_Database` => `lzma:9`
- `Binary_Chunked`, `Archive_TAR`, `Unknown` => `zstd` high level
- `Archive_ZIP`, `Archive_7Z` => extract-then-compress workflow

## Benchmark Integration

`setBenchmarkData()` accepts benchmark rows and groups them by `FileType`. For known types:

- best ratio => primary selection
- second best => fallback selection

## JSON Config

Rules are serializable with:

- `saveRules(path)`
- `loadRules(path)`

Example config: `config/selection_rules.json`.

## Why This Design

- Deterministic defaults for reproducibility
- Data-driven override path for future tuning
- Explicit rationale string for explainable CLI output
- Safe behavior for already-compressed media
