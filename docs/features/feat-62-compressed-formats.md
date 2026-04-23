# Worker Spec: Compressed Formats Benchmark Corpus

**Issue:** #62
**Branch:** `feat/compressed-formats-corpus`
**Type:** Feature - Benchmark Extension

## Overview

Establish benchmark corpus for already-compressed file formats (JPEG, PNG, video) to validate the intelligent algorithm selector and populate decision database.

## Current Context

### Available Resources
- **JPEG:** 881 files (139 MB) in `/sources/images/`
- **PNG:** 17 files (16 MB) in `/sources/images/`
- **oxipng preprocessor:** Merged in #57, needs testing
- **Intelligent Selector:** Implemented in #60, needs validation on compressed formats

### Gap
Current benchmarks only cover RAW files (8 RAF). No data on compressed format behavior.

## Implementation Plan

### Phase 1: JPEG Subset Selection

**Goal:** Select 50-100 representative JPEG files

**Selection Criteria:**
```cpp
// Categorize by size
namespace SizeCategory {
    constexpr size_t Small = 100 * 1024;    // < 100KB
    constexpr size_t Medium = 1024 * 1024;   // 100KB - 1MB
    constexpr size_t Large = 10 * 1024 * 1024; // 1MB - 10MB
}
```

**Files to Create/Modify:**
- `src/mosqueeze-bench/tools/corpus_selector.cpp` - Select representative files
- `scripts/select_corpus.py` - Python helper for corpus selection

### Phase 2: Run Benchmarks

**Command:**
```cmd
# JPEG benchmark (no preprocessing - already compressed)
mosqueeze-bench -d "G:\mosqueeze-bench\sources\images" -a zstd,xz,brotli,zpaq --default-only -o results/compressed-jpeg --json --csv -v --threads 4

# PNG without oxipng
mosqueeze-bench -d "G:\mosqueeze-bench\sources\images" -a zstd,xz,brotli,zpaq -g "*.png" --default-only -o results/png-no-prep --json --csv -v

# PNG with oxipng preprocessing
mosqueeze-bench -d "G:\mosqueeze-bench\sources\images" -a zstd,xz,brotli,zpaq --preprocess oxipng -g "*.png" --default-only -o results/png-oxipng --json --csv -v
```

**Expected Output:**
- `results/compressed-jpeg/results.json` - Full benchmark data
- `results/compressed-jpeg/results.csv` - Tabular format
- `results/compressed-jpeg/results.sqlite3` - Queryable database

### Phase 3: Analysis Queries

**Add to `docs/queries/`:**
```sql
-- Average ratio by file type
SELECT file_type, algorithm, level, 
       AVG(ratio) as avg_ratio,
       AVG(encode_ms) as avg_encode_ms
FROM results 
WHERE file LIKE '%.jpg'
GROUP BY file_type, algorithm, level
ORDER BY avg_ratio DESC;

-- Size category analysis
SELECT 
    CASE 
        WHEN original_bytes < 100000 THEN 'small'
        WHEN original_bytes < 1000000 THEN 'medium'
        ELSE 'large'
    END as size_cat,
    algorithm, AVG(ratio)
FROM results
GROUP BY size_cat, algorithm;
```

## Expected Results

| Format | ZPAQ/5 Ratio | ZSTD/22 Ratio | Notes |
|--------|--------------|---------------|-------|
| JPEG | ~1.01-1.02x | ~1.005-1.01x | Minimal gain, ZPAQ best |
| PNG (raw) | ~1.01-1.02x | ~1.01x | depends on image content |
| PNG (oxipng) | ~1.02-1.03x | ~1.015x | Metadata stripped improves ratio |

## Files to Create/Modify

```
docs/features/feat-62-compressed-formats.md (this file)
src/mosqueeze-bench/tools/corpus_selector.cpp (optional)
scripts/select_corpus.py (optional helper)
docs/queries/compressed_formats_analysis.sql
```

## Acceptance Criteria

- [ ] JPEG subset benchmark complete (50+ files recommended)
- [ ] PNG benchmark with and without oxipng preprocessing
- [ ] Results stored in SQLite database
- [ ] Query demonstrates: "ZPAQ/5 is optimal for JPEG cold storage"
- [ ] Intelligent selector rules updated for compressed formats

## Testing

```bash
# Verify oxipng preprocessor works
mosqueeze-bench -f test.png -a zstd --preprocess oxipng -v

# Verify database has results
sqlite3 results.sqlite3 "SELECT COUNT(*) FROM results WHERE file LIKE '%.jpg'"
```

## Dependencies

- #57: oxipng integration (merged ✓)
- #55: Intelligent Algorithm Selection Engine (merged ✓)

## Estimated Effort

- Phase 1 (JPEG selection): 1-2 hours
- Phase 2 (Run benchmarks): 2-4 hours (depends on corpus size)
- Phase 3 (Analysis): 1-2 hours
