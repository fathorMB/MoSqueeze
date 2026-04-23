# JPEG Benchmark Results

**Date**: 2026-04-23  
**Corpus**: 49 JPEG files  
**Purpose**: Confirm hypothesis that JPEG compression is not beneficial

## Executive Summary

**JPEG compression is NOT recommended** - standard algorithms make 60-70% of files larger.

| Algorithm | Files WORSE | Files Better | Avg Ratio | Time |
|-----------|-------------|--------------|-----------|------|
| ZSTD/22 | **61%** | 16% | 1.008x | 379ms |
| XZ/9 | **67%** | 14% | 1.007x | 258ms |
| BROTLI/11 | **71%** | 27% | 1.010x | 511ms |
| ZPAQ/5 | 0% | 39% | 1.018x | 788ms |

## Key Findings

### 1. Standard Algorithms Are Harmful

- ZSTD/22: 61% of JPEG files get BIGGER after compression
- XZ/9: 67% of JPEG files get BIGGER after compression  
- BROTLI/11: 71% of JPEG files get BIGGER after compression

**Why?** JPEG uses lossy DCT compression. Output has high entropy (7.95+). Compression overhead often exceeds savings.

### 2. ZPAQ Is Safe But Not Worth It

- Files that get bigger: **0%**
- Average compression: **1.8%**
- Encode time: **788ms**

Not cost-effective: 788ms for 1.8% gain.

### 3. Low Entropy Exception

Files with entropy < 7.9 compress significantly better:

| Entropy Range | Avg Ratio | Files |
|---------------|-----------|-------|
| < 7.9 | **1.155x** (15.5% gain) | 2 |
| 7.9 - 7.95 | 1.030x (3% gain) | 6 |
| > 7.95 | 1.010x (1% gain) | 41 |

## Decision Rules

```
JPEG -> SKIP (compression makes most files larger)
EXCEPTION: If entropy < 7.9, consider ZPAQ/5
```

## Comparison: JPEG vs PNG

| Format | Avg Ratio | Best Ratio | Worth It? |
|--------|-----------|------------|-----------|
| **PNG** | 1.091x | 1.120x | **YES** (9-12% gain) |
| **JPEG** | 1.010x | 1.189x | **NO** (1-2% gain, risk of expansion) |

## Recommendations

- **Do NOT compress JPEG files**
- Archive as-is
- Storage savings: ~0% (negligible)

## Raw Data

- Location: G:\mosqueeze-bench\results\jpeg-baseline\
- Measurements: 196 (49 files x 4 algorithms)
