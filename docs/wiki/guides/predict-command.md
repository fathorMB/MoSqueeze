# Compression Prediction (`mosqueeze predict`)

**Summary**: Get compression recommendations for files before compressing. Uses 753 benchmark results to predict algorithm, level, estimated ratio, and speed.

**Last updated**: 2026-04-24

---

## Command

```bash
mosqueeze predict <file...> [options]
```

### Options

| Flag | Description |
|------|-------------|
| `-o, --output <file>` | Write output to file instead of stdout |
| `-f, --format <format>` | Output format: `text` (default), `json` |
| `-s, --prefer-speed` | Prioritize fast algorithms over best ratio |
| `--decision-matrix <path>` | Custom decision matrix JSON (default: embedded) |
| `-v, --verbose` | Show debug information |

---

## How It Works

The prediction pipeline combines:

1. **Decision Matrix**: 753 benchmark results across 34 file types
2. **File Type Detection**: MIME type and extension analysis
3. **Preprocessor Selection**: JSON/XML canonicalization, Bayer preprocessing
4. **Skip Detection**: Already-compressed files (JPEG, MP4, etc.)

### Prediction Flow

```
File → FileTypeDetector → DecisionMatrix → Predictions → Output
         ↓                      ↓
    MIME Type            3 recommendations:
                          1. Best ratio (slow)
                          2. Fastest (< 100ms)
                          3. Balanced (ratio/speed tradeoff)
```

---

## Examples

### Text Output (Default)

```bash
$ mosqueeze predict document.json
```

```
File: document.json
Size: 1048576 bytes
Type: application/json
Preprocessor: json-canonical

Recommendations:
  1. lzma level 9 [slow] 8.5x ratio
     Estimated size: 123360 bytes (88.2% compression)
     Fallback: zstd level 3 (5.4x)
  2. brotli level 11 [medium] 7.2x ratio
     Estimated size: 145635 bytes (86.1% compression)
  3. zstd level 3 [fast] 5.4x ratio
     Estimated size: 194176 bytes (81.5% compression)
```

### JSON Output (for API integration)

```bash
$ mosqueeze predict video.mp4 --format json
```

```json
{
  "file": "video.mp4",
  "extension": ".mp4",
  "input_size": 52428800,
  "should_skip": true,
  "skip_reason": "Already-compressed or incompressible format .mp4 (avg. ratio 1.000x)"
}
```

### Multiple Files

```bash
$ mosqueeze predict file1.json file2.xml file3.db --format json
```

```json
[
  {
    "file": "file1.json",
    "extension": ".json",
    "input_size": 1024,
    "recommendations": [...]
  },
  {
    "file": "file2.xml",
    "extension": ".xml",
    "input_size": 2048,
    "recommendations": [...]
  },
  ...
]
```

### Prefer Speed

```bash
$ mosqueeze predict large_file.bin --prefer-speed
```

Output sorted by speed (fastest first).

---

## Supported File Types

The decision matrix includes benchmark data for:

### Compressible (High Ratio)

| Extension | Best Algorithm | Best Ratio |
|-----------|---------------|------------|
| `.json` | lzma/9 | 8.5x |
| `.xml` | lzma/9 | 7.8x |
| `.txt`, `.md` | brotli/11 | 6.2x |
| `.cpp`, `.py`, `.js` | zpaq/7 | 5.5x |
| `.db`, `.sqlite` | zpaq/5 | 4.8x |
| `.log` | zstd/22 | 4.2x |
| `.csv` | lzma/9 | 5.1x |
| `.html` | brotli/11 | 6.8x |

### Already Compressed (Skip)

| Extension | Reason |
|-----------|--------|
| `.jpg`, `.jpeg` | Already lossy compressed |
| `.mp4`, `.mov` | H.264/265 codec |
| `.png` | Already compressed |
| `.7z`, `.gz` | Archive format |
| `.mp3`, `.aac` | Audio codec |

### Partial Compression

| Extension | Best Algorithm | Best Ratio |
|-----------|---------------|------------|
| `.pdf` | zstd/19 | 1.2x |
| `.avi` | zpaq/5 | 2.5x |

---

## JSON Output Schema

```json
{
  "file": "string (file path)",
  "extension": "string (.ext)",
  "input_size": "number (bytes)",
  "estimated_output_size": "number (bytes, primary recommendation)",
  "should_skip": "boolean",
  "skip_reason": "string (if should_skip is true)",
  "mime_type": "string",
  "preprocessor": "string (none, json-canonical, xml-canonical, bayer-raw)",
  "preprocessor_available": "boolean",
  "recommendations": [
    {
      "algorithm": "string (zstd, brotli, lzma, zpaq)",
      "level": "number",
      "estimated_ratio": "number",
      "estimated_size": "number (bytes)",
      "speed": "string (fast, medium, slow)",
      "fallback_algorithm": "string (optional)",
      "fallback_level": "number (optional)",
      "fallback_ratio": "number (optional)"
    }
  ]
}
```

---

## Integration with TheVault

TheVault calls `mosqueeze predict --format json` during file upload:

```go
// internal/mosqueeze/client.go
func (c *Client) Predict(filePath string) (*Prediction, error) {
    cmd := exec.Command("mosqueeze", "predict", filePath, "--format", "json")
    output, err := cmd.Output()
    if err != nil {
        return nil, fmt.Errorf("predict command failed: %w", err)
    }
    
    var pred Prediction
    if err := json.Unmarshal(output, &pred); err != nil {
        return nil, fmt.Errorf("parse prediction: %w", err)
    }
    return &pred, nil
}
```

See TheVault #59, #60, #61, #62 for the full integration.

---

## Performance

Prediction is **instant** (< 1ms) for any file:

1. Extension lookup in embedded benchmark data
2. No file content analysis (uses extension + metadata)
3. Decision matrix is compiled into the binary

For large files, consider using `--prefer-speed` to prioritize fast compression algorithms.

---

## Notes

- **Accuracy**: Estimated ratios are based on April 2026 benchmarks (753 tests, 34 file types)
- **Custom Matrix**: Use `--decision-matrix` to provide organization-specific benchmark data
- **Preprocessors**: JSON/XML files are automatically canonicalized before compression
- **RAW Images**: Bayer preprocessing available for `.raf`, `.arw`, `.cr2`, `.nef`, `.dng`