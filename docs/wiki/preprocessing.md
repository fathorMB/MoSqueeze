# Preprocessing for Better Compression

## Overview

MoSqueeze supports a reversible preprocessing stage before compression.

Current implementation status:

- `json-canonical`, `xml-canonical`: canonicalize and store original bytes for exact postprocess restoration.
- `image-meta-strip`: currently pass-through transform for JPEG/PNG/RAW routing (safe, reversible baseline).
- `png-optimizer`: PNG-only recompression with selectable `libpng`/`oxipng` engine (lossless).
- `bayer-raw`: reversible byte-plane transform for `Image_Raw` payloads (experimental baseline).
- `zstd-dict`: training/load flow implemented; preprocess step currently pass-through.

## Bayer RAW (RAF) Parsing Fix

`bayer-raw` now parses Fuji RAF structure and transforms only the RAW Bayer image region.

Before this fix, the transform was applied to the entire file (header + metadata + image payload), which could reduce compression quality and risk structural corruption for camera-specific containers.

Current behavior:

- RAF header/metadata/trailing sections are preserved byte-for-byte
- Bayer byte-plane transform is applied only to the detected RAW image segment
- unparseable/non-RAF RAW payloads safely fall back to pass-through (`metadata version 0`)
- postprocess supports both new region-based metadata (`version 2`) and legacy full-file metadata (`version 1`)

## Available Preprocessors

| Name | File Types | Typical Improvement (Target) |
|------|------------|-------------------------------|
| `json-canonical` | Structured text (JSON-like) | 10-25% |
| `xml-canonical` | Structured text (XML-like) | 5-15% |
| `image-meta-strip` | JPEG, PNG, TIFF-based RAW (NEF/CR2/CR3/RAF/ARW/DNG/ORF/RW2) | Routing enabled; stripping logic staged |
| `png-optimizer` | PNG | 5-30% (dataset-dependent) |
| `bayer-raw` | RAW Bayer streams (Image_Raw) | Experimental reversible transform (dataset-dependent) |
| `zstd-dict` | Similar datasets after training | 15-35% |

## CLI

```bash
# List currently registered preprocessors
mosqueeze --list-preprocessors

# Benchmark with preprocessing enabled
mosqueeze-bench --directory ./datasets --preprocess auto --default-only

# Force PNG optimizer with oxipng (if installed in PATH; otherwise libpng fallback)
mosqueeze-bench --directory ./images --preprocess png-optimizer --png-engine oxipng --png-level 3
```

## Pipeline Format

Compressed payload is followed by a trailing header:

```text
[compressed data]
[4 bytes magic: "MSQF"]
[1 byte preprocessor type]
[4 bytes metadata length, little-endian]
[N bytes metadata]
```

This enables decompression to auto-detect and apply postprocessing.
