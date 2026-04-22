# Preprocessing for Better Compression

## Overview

MoSqueeze supports a reversible preprocessing stage before compression.

## Available Preprocessors

| Name | File Types | Typical Improvement (Target) |
|------|------------|-------------------------------|
| `json-canonical` | Structured text (JSON-like) | 10-25% |
| `xml-canonical` | Structured text (XML-like) | 5-15% |
| `image-meta-strip` | JPEG, PNG | 2-10% |
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
