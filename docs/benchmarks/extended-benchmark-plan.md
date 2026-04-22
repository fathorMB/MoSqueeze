# Extended Benchmark Plan for Intelligent Algorithm Selection

**Goal:** Build a comprehensive benchmark database to power the Intelligent Algorithm Selection Engine.

**Duration:** 1-2 weeks (depending on hardware and corpus size)

---

## Overview

We need to benchmark **ALL combinations** of:
- **File types** → 500+ files across 8 categories
- **Preprocessors** → 6 options (none + 5 preprocessors)
- **Algorithms** → 4 options (zstd, xz, brotli, zpaq)
- **Levels** → 5-6 levels per algorithm

**Estimated combinations:** 500 files × 6 preprocessors × 4 algorithms × 5 levels = **~60,000 benchmark entries**

---

## Benchmark Corpus Structure

### Directory Layout

```
benchmark-corpus/
├── images/
│   ├── raw/
│   │   ├── fuji/
│   │   │   ├── uncompressed/     # Need to source
│   │   │   │   ├── fuji_uncompressed_001.raf
│   │   │   │   ├── fuji_uncompressed_002.raf
│   │   │   │   └── ...
│   │   │   └── lossless-compressed/  # Already have
│   │   │       ├── fuji_lossless_001.raf
│   │   │       └── ...
│   │   ├── sony/
│   │   │   ├── uncompressed/
│   │   │   └── compressed/
│   │   ├── nikon/
│   │   ├── canon/
│   │   ├── phase-one/
│   │   └── hasselblad/
│   ├── png/
│   │   ├── screenshots/
│   │   │   ├── screenshot_ui_001.png
│   │   │   ├── screenshot_ui_002.png
│   │   │   └── ...
│   │   ├── photos/
│   │   │   ├── photo_png_001.png
│   │   │   └── ...
│   │   ├── graphics/
│   │   │   ├── logo_001.png
│   │   │   ├── icon_001.png
│   │   │   └── ...
│   │   └── metadata-heavy/       # EXIF, XMP, ICC profiles
│   │       ├── heavy_meta_001.png
│   │       └── ...
│   ├── jpeg/
│   │   ├── photos/
│   │   ├── compressed/
│   │   └── progressive/
│   ├── webp/
│   │   ├── lossless/
│   │   └── lossy/
│   └── bmp/
│       └── uncompressed/
├── documents/
│   ├── json/
│   │   ├── api-responses/
│   │   │   ├── rest_api_001.json
│   │   │   └── ...
│   │   ├── configs/
│   │   │   ├── config_large_001.json
│   │   │   └── ...
│   │   ├── datasets/
│   │   │   ├── dataset_001.json
│   │   │   └── ...
│   │   └── minified/
│   │       ├── minified_large_001.json
│   │       └── ...
│   ├── xml/
│   │   ├── rss-feeds/
│   │   ├── soap-messages/
│   │   ├── svg-graphics/
│   │   └── configs/
│   ├── html/
│   │   ├── pages/
│   │   ├── cached/
│   │   └── minified/
│   ├── csv/
│   │   ├── small/
│   │   ├── medium/
│   │   └── large/
│   └── markdown/
│       ├── docs/
│       └── readme/
├── source-code/
│   ├── javascript/
│   │   ├── unminified/
│   │   └── minified/
│   ├── python/
│   │   ├── scripts/
│   │   └── libraries/
│   ├── cpp/
│   │   ├── headers/
│   │   └── source/
│   ├── rust/
│   └── go/
├── archives/
│   ├── tar/
│   │   ├── uncompressed/
│   │   └── gzip-compressed/
│   ├── zip/
│   │   ├── deflate/
│   │   └── stored/
│   └── 7z/
│       └── lzma/
├── binary/
│   ├── executables/
│   │   ├── linux/
│   │   ├── windows/
│   │   └── macos/
│   ├── libraries/
│   │   ├── dll/
│   │   └── so/
│   └── databases/
│       ├── sqlite/
│       └── custom/
└── special/
    ├── already-compressed/      # Already compressed files
    │   ├── video/
    │   ├── audio/
    │   └── encrypted/
    ├── high-entropy/            # Random data
    │   ├── random_1mb.bin
    │   ├── random_10mb.bin
    │   └── random_100mb.bin
    └── edge-cases/
        ├── empty.bin
        ├── single_byte.bin
        ├── repeated_00.bin      # All 0x00
        ├── repeated_ff.bin      # All 0xFF
        └── repeated_pattern.bin # Repeated pattern
```

---

## File Requirements per Category

### Images (200 files)

| Category | Count | Size Range | Notes |
|----------|-------|------------|-------|
| **RAW - Uncompressed** | 30 | 20-80 MB | Phase One IIQ, uncompressed ARW/NEF/CR2 |
| **RAW - Lossless Compressed** | 30 | 10-40 MB | Fujifilm RAF, Sony ARW compressed |
| **PNG - Screenshots** | 30 | 100KB-5MB | UI screenshots, high color coherence |
| **PNG - Photos** | 20 | 1-10 MB | Photo exports, varied entropy |
| **PNG - Graphics** | 20 | 10KB-2MB | Logos, icons, flat colors |
| **PNG - Metadata** | 20 | 50KB-3MB | Heavy EXIF/XMP/ICC |
| **JPEG** | 30 | 50KB-5MB | Various quality levels |
| **WebP** | 20 | 50KB-3MB | Lossless and lossy |

### Documents (100 files)

| Category | Count | Size Range | Notes |
|----------|-------|------------|-------|
| **JSON - API** | 25 | 1KB-5MB | REST responses, nested objects |
| **JSON - Configs** | 20 | 1KB-500KB | Package.json, configs |
| **JSON - Datasets** | 15 | 1-50 MB | Large datasets |
| **JSON - Minified** | 15 | Varied | Already minified |
| **XML** | 25 | 1KB-10MB | RSS, SOAP, SVG |

### Source Code (80 files)

| Category | Count | Size Range | Notes |
|----------|-------|------------|-------|
| **JavaScript Unminified** | 20 | 10KB-2MB | Readable source |
| **JavaScript Minified** | 20 | 5KB-500KB | Already minified |
| **Python** | 15 | 5KB-500KB | Scripts and modules |
| **C++** | 15 | 5KB-1MB | Headers and source |
| **Other** | 10 | Varied | Rust, Go, etc. |

### Archives (40 files)

| Category | Count | Size Range | Notes |
|----------|-------|------------|-------|
| **TAR Uncompressed** | 10 | 1-50 MB | Pure tar, good for testing |
| **TAR.GZ** | 10 | 100KB-10MB | Already compressed |
| **ZIP** | 10 | 100KB-20MB | Mixed compression |
| **7Z** | 10 | 100KB-15MB | LZMA compressed |

### Binary (40 files)

| Category | Count | Size Range | Notes |
|----------|-------|------------|-------|
| **Executables** | 15 | 50KB-50MB | PE, ELF, Mach-O |
| **Libraries** | 10 | 50KB-20MB | DLL, SO, DYLIB |
| **Databases** | 15 | 1MB-100MB | SQLite, etc. |

### Special (40 files)

| Category | Count | Size Range | Notes |
|----------|-------|------------|-------|
| **Already Compressed** | 15 | Varied | Video, audio, encrypted |
| **High Entropy** | 10 | 1-100 MB | Random data (worst case) |
| **Edge Cases** | 15 | 0-10 MB | Empty, repeated, patterns |

---

## Combinations to Test

### Preprocessors

| Preprocessor | ID | Applicable File Types |
|--------------|----|--------------------|
| None | `none` | All files |
| JsonCanonicalizer | `json-canon` | JSON files |
| XmlCanonicalizer | `xml-canon` | XML, HTML, SVG |
| PngOptimizer | `png-optimizer` | PNG files |
| ImageMetaStripper | `image-meta-stripper` | JPEG, PNG, WebP, RAW |
| BayerPreprocessor | `bayer-raw` | Uncompressed RAW files |

### Algorithms & Levels

| Algorithm | Levels | Notes |
|-----------|--------|-------|
| **zstd** | 1, 3, 9, 15, 19 | Fast to ultra |
| **xz** | 1, 3, 6, 9 | LZMA2 |
| **brotli** | 1, 4, 7, 9, 11 | Web compression |
| **zpaq** | 1, 2, 3, 4, 5 | Extreme compression |

## Benchmark Matrix

### Primary Matrix (All Combinations)

For each file:
- Test all applicable preprocessors
- Test all algorithms
- Test all levels

Example for `photo.json`:
```
photo.json × json-canon × {zstd, xz, brotli, zpaq} × {multiple levels}
photo.json × none × {zstd, xz, brotli, zpaq} × {multiple levels}
```

### Baseline (No Preprocessor)

Every file gets tested with:
```
file × none × {zstd, xz, brotli, zpaq} × {all levels}
```

This establishes baseline compression for each file type.

### Preprocessor-Specific

Only applicable files:

| File Type | Preprocessors to Test |
|-----------|----------------------|
| JSON | none, json-canon |
| XML/HTML | none, xml-canon |
| PNG | none, png-optimizer, image-meta-stripper |
| JPEG | none, image-meta-stripper |
| RAW (uncompressed) | none, bayer-raw, image-meta-stripper |
| RAW (compressed) | none, image-meta-stripper |
| Other | none only |

---

## Metrics to Collect

For each benchmark run, record:

```cpp
struct BenchmarkResult {
    // File identification
    std::string filePath;
    std::string fileHash;          // SHA256
    size_t fileSize;
    std::string detectedType;       // MIME type
    std::string extension;
    
    // File features
    double entropy;
    double entropyVariance;
    size_t repeatPatterns;
    double chunkRatio;
    bool isStructured;
    
    // Compression parameters
    std::string preprocessor;
    std::string algorithm;
    int level;
    
    // Results
    size_t compressedSize;
    double ratio;                  // compressedSize / fileSize
    
    // Timing
    std::chrono::milliseconds preprocessTime;
    std::chrono::milliseconds compressionTime;
    std::chrono::milliseconds decompressionTime;
    std::chrono::milliseconds totalTime;
    
    // Resources
    size_t peakMemoryBytes;
    size_t avgCpuPercent;
    
    // Verification
    bool roundTripVerified;        // Decompression successful
    std::string error;             // If failed
};
```

---

## Benchmark Tool Implementation

### New CLI Command

```bash
mosqueeze-benchmark-extended \
    --corpus ./benchmark-corpus/ \
    --output ./benchmark-results.db \
    --preprocessors all \
    --algorithms zstd,xz,brotli,zpaq \
    --levels auto \
    --parallel 8 \
    --verify-roundtrip
```

### Configuration

```yaml
# benchmark-config.yaml
corpus:
  path: "./benchmark-corpus/"
  recursive: true
  
output:
  database: "./benchmark-results.db"
  format: sqlite
  
combinations:
  preprocessors:
    - id: none
      files: "*"
    - id: json-canon
      files: "*.json"
    - id: xml-canon
      files: ["*.xml", "*.html", "*.svg"]
    - id: png-optimizer
      files: "*.png"
    - id: image-meta-stripper
      files: ["*.png", "*.jpg", "*.jpeg", "*.webp", "*.raf", "*.arw"]
    - id: bayer-raw
      files: ["*.raf", "*.arw", "*.nef", "*.cr2", "*.iiq"]
      condition: is_uncompressed_raw
  
  algorithms:
    - name: zstd
      levels: [1, 3, 9, 15, 19]
    - name: xz
      levels: [1, 3, 6, 9]
    - name: brotli
      levels: [1, 4, 7, 9, 11]
    - name: zpaq
      levels: [1, 2, 3, 4, 5]
  
  parallel: 8
  verify_roundtrip: true
  memory_limit_mb: 4096
```

---

## Execution Plan

### Phase 0a: Corpus Preparation (2-3 days)

**Tasks:**
1. [ ] Create directory structure
2. [ ] Source RAW files (uncompressed formats)
   - Phase One IIQ samples
   - Sony ARW uncompressed
   - Nikon NEF uncompressed
   - Canon CR2/CR3 uncompressed
3. [ ] Generate synthetic files
   - High entropy random files
   - Repeated pattern files
   - Edge cases (empty, single byte)
4. [ ] Download public datasets
   - JSON datasets (GitHub API responses)
   - XML datasets (Stack Overflow dumps)
   - Source code repositories
5. [ ] Create file manifest
   - Hash all files
   - Record expected size
   - Verify diversity

### Phase 0b: Benchmark Tool (3-4 days)

**Tasks:**
1. [ ] Implement `BenchmarkRunner` class
   - Load configuration
   - Generate test matrix
   - Execute benchmarks
   - Record results
2. [ ] Add parallel execution
   - Thread pool
   - Memory limits
   - Progress reporting
3. [ ] Add round-trip verification
   - Decompress result
   - Verify hash matches
4. [ ] Add result storage
   - SQLite database
   - Append mode for incremental runs
5. [ ] Add resume capability
   - Skip already-tested files
   - Checkpoint system

### Phase 0c: Benchmark Execution (varies by hardware)

**Estimated Time:**

| Corpus Size | Parallelism | Estimated Time |
|-------------|-------------|----------------|
| 500 files × 60 combinations × 3s avg | 8 threads | ~4 hours |
| 500 files × 60 combinations × 5s avg | 4 threads | ~10 hours |
| 500 files × 60 combinations × 10s avg | 2 threads | ~42 hours |

**Reality check:** Some files (large RAW, archives) take 30-60s per combination.

**Realistic estimate:** 24-48 hours on 8-core machine

**Tasks:**
1. [ ] Run baseline (no preprocessor) on all files
2. [ ] Run preprocessor-specific tests
3. [ ] Verify all round-trips passed
4. [ ] Export to SQLite database

### Phase 0d: Database Generation (1 day)

**Tasks:**
1. [ ] Create SQLite schema
2. [ ] Import all results
3. [ ] Create indexes
4. [ ] Create views for common queries
5. [ ] Verify query performance (<100ms)
6. [ ] Bundle with library (~20-50MB)

---

## Database Generation Script

```python
#!/usr/bin/env python3
"""
generate_benchmark_db.py

Generate SQLite database from benchmark results.
"""

import sqlite3
import hashlib
import json
from pathlib import Path
from datetime import datetime

def calculate_entropy(data: bytes) -> float:
    """Calculate Shannon entropy in bits per byte."""
    import math
    from collections import Counter
    if not data:
        return 0.0
    counts = Counter(data)
    total = len(data)
    entropy = 0.0
    for count in counts.values():
        p = count / total
        entropy -= p * math.log2(p)
    return entropy

def analyze_file(filepath: Path) -> dict:
    """Extract file features."""
    data = filepath.read_bytes()
    return {
        'file_hash': hashlib.sha256(data).hexdigest(),
        'file_size': len(data),
        'entropy': calculate_entropy(data),
        'extension': filepath.suffix.lower(),
        'detected_type': detect_mime_type(data),
    }

def import_results(db_path: Path, results_dir: Path):
    """Import benchmark results into SQLite."""
    conn = sqlite3.connect(db_path)
    conn.executescript('''
        CREATE TABLE IF NOT EXISTS files (
            id INTEGER PRIMARY KEY,
            file_hash TEXT UNIQUE,
            file_type TEXT,
            extension TEXT,
            file_size INTEGER,
            entropy REAL,
            detected_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
        
        CREATE TABLE IF NOT EXISTS results (
            id INTEGER PRIMARY KEY,
            file_id INTEGER REFERENCES files(id),
            preprocessor TEXT,
            algorithm TEXT,
            compression_level INTEGER,
            compressed_size INTEGER,
            ratio REAL,
            compression_time_ms INTEGER,
            decompression_time_ms INTEGER,
            peak_memory_kb INTEGER,
            roundtrip_ok INTEGER,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
        
        CREATE INDEX IF NOT EXISTS idx_files_type ON files(file_type);
        CREATE INDEX IF NOT EXISTS idx_files_hash ON files(file_hash);
        CREATE INDEX IF NOT EXISTS idx_results_algo ON results(algorithm);
        CREATE INDEX IF NOT EXISTS idx_results_ratio ON results(ratio);
    ''')
    
    # Import results...
    for result_file in results_dir.glob('*.json'):
        result = json.loads(result_file.read_text())
        
        # Analyze file
        features = analyze_file(Path(result['file_path']))
        
        # Insert file
        cursor = conn.execute('''
            INSERT OR IGNORE INTO files (file_hash, file_type, extension, file_size, entropy)
            VALUES (?, ?, ?, ?, ?)
        ''', (features['file_hash'], features['detected_type'], 
              features['extension'], features['file_size'], features['entropy']))
        
        file_id = cursor.lastrowid or conn.execute(
            'SELECT id FROM files WHERE file_hash = ?', 
            (features['file_hash'],)
        ).fetchone()[0]
        
        # Insert result
        conn.execute('''
            INSERT INTO results (file_id, preprocessor, algorithm, compression_level,
                               compressed_size, ratio, compression_time_ms, 
                               decompression_time_ms, peak_memory_kb, roundtrip_ok)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ''', (file_id, result['preprocessor'], result['algorithm'], result['level'],
              result['compressed_size'], result['ratio'], result['compression_time_ms'],
              result['decompression_time_ms'], result['peak_memory_kb'], 
              1 if result['roundtrip_ok'] else 0))
    
    conn.commit()
    
    # Create views
    conn.executescript('''
        CREATE VIEW IF NOT EXISTS best_results AS
        SELECT 
            f.file_type, f.extension, f.file_size, f.entropy,
            r.preprocessor, r.algorithm, r.compression_level,
            r.ratio, r.compression_time_ms
        FROM files f
        JOIN results r ON f.id = r.file_id
        WHERE r.ratio = (
            SELECT MIN(r2.ratio) FROM results r2 WHERE r2.file_id = f.id
        );
        
        CREATE VIEW IF NOT EXISTS algo_performance AS
        SELECT 
            f.file_type,
            r.algorithm,
            r.compression_level,
            AVG(r.ratio) as avg_ratio,
            AVG(r.compression_time_ms) as avg_time_ms,
            COUNT(*) as sample_count
        FROM files f
        JOIN results r ON f.id = r.file_id
        GROUP BY f.file_type, r.algorithm, r.compression_level
        ORDER BY f.file_type, avg_ratio;
    ''')
    
    conn.close()

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--results-dir', required=True)
    parser.add_argument('--output', default='benchmark.db')
    args = parser.parse_args()
    
    import_results(Path(args.output), Path(args.results_dir))
```

---

## Validation

After benchmark completion, verify:

1. **Coverage**
   - All file types have results
   - All algorithms tested on all applicable files
   - No missing combinations

2. **Data Quality**
   - All round-trips verified
   - No anomalies (ratio > 1.5 or < 0.01 without reason)
   - Time measurements reasonable

3. **Usefulness**
   - Query: "What's best for PNG?" returns sensible answer
   - Query: "What's best for JSON?" returns sensible answer
   - Confidence scores correlate with accuracy

---

## Estimated Resource Requirements

| Resource | Requirement |
|----------|-------------|
| **CPU** | 4-8 cores recommended |
| **RAM** | 8-16 GB (for parallel execution) |
| **Disk** | 100-200 GB (corpus + results) |
| **Time** | 24-48 hours (full run) |
| **Database size** | 20-50 MB (final compressed) |

---

## Deliverables

1. **Benchmark corpus** (~20 GB compressed)
2. **Benchmark database** (~50 MB SQLite)
3. **Benchmark tool** (`mosqueeze-benchmark-extended`)
4. **Documentation** (how to reproduce, extend)

---

## Next Steps

1. Source uncompressed RAW files (critical for BayerPreprocessor testing)
2. Build benchmark tool
3. Run benchmarks on dedicated machine
4. Generate database
5. Bundle with MoSqueeze

---

## Questions to Resolve

1. **Where to get uncompressed RAW files?**
   - Phase One IIQ from medium format cameras
   - Sony ARW uncompressed from Sony cameras
   - Generate synthetic RAW?

2. **How to distribute corpus?**
   - Too large for git
   - Separate download (S3, GitHub releases)
   - Generate synthetic for common cases?

3. **Skip already-compressed files?**
   - Video/audio files (already compressed)
   - Encrypted files
   - Or include for completeness?

4. **Hardware for benchmark run?**
   - Need 8+ core machine
   - Cloud instance vs dedicated hardware
   - Estimated cloud cost: $50-100