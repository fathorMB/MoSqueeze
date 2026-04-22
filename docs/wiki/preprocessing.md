# Preprocessing for Better Compression

## Overview

MoSqueeze supports a reversible preprocessing stage before compression.

Current implementation status:

- `json-canonical`, `xml-canonical`: canonicalize and store original bytes for exact postprocess restoration.
- `image-meta-strip`: currently pass-through transform for JPEG/PNG/RAW routing (safe, reversible baseline).
- `bayer-raw`: reversible byte-plane transform for `Image_Raw` payloads (experimental baseline).
- `zstd-dict`: training/load flow implemented; preprocess step currently pass-through.

## Available Preprocessors

| Name | File Types | Typical Improvement (Target) |
|------|------------|-------------------------------|
| `json-canonical` | Structured text (JSON-like) | 10-25% |
| `xml-canonical` | Structured text (XML-like) | 5-15% |
| `image-meta-strip` | JPEG, PNG, TIFF-based RAW (NEF/CR2/CR3/RAF/ARW/DNG/ORF/RW2) | Routing enabled; stripping logic staged |
| `bayer-raw` | RAW Bayer streams (Image_Raw) | Experimental reversible transform (dataset-dependent) |
| `zstd-dict` | Similar datasets after training | 15-35% |

## CLI

```bash
# List currently registered preprocessors
mosqueeze --list-preprocessors
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
