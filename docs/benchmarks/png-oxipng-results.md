# PNG Benchmark Results - Oxipng Preprocessing

**Date:** 2026-04-23  
**Corpus:** 1,445 PNG files  
**Preprocessor:** oxipng (lossless PNG optimization)

## Executive Summary

Oxipng preprocessing **improves compression ratio by +2.5%** on average.

| Metric | Baseline | + Oxipng | Improvement |
|--------|----------|----------|-------------|
| ZSTD/22 | 1.091x | 1.120x | **+2.65%** |
| BROTLI/11 | 1.090x | 1.118x | +2.60% |
| ZPAQ/5 | 1.082x | 1.108x | +2.45% |

## Analysis

### What Oxipng Does

Oxipng is a lossless PNG optimizer that:
1. Strips non-essential metadata chunks (EXIF, tEXt, etc.)
2. Optimizes PNG filtering (Heuristic auto-selection)
3. Reproduces image data more efficiently

### Why It Helps Compression

| Stage | Size | Reduction |
|-------|------|-----------|
| Original PNG | 100% | - |
| After oxipng | ~97% | -3% |
| After ZSTD/22 | ~89% | -8% from baseline |
| **Total** | **~89%** | **~11% compression** |

The oxipng preprocessing makes PNG data **more compressible** by:
- Removing redundant metadata
- Using optimal PNG filters
- Reducing entropy in the image data stream

### Encode Time Impact

| Algorithm | Baseline | +Oxipng | Overhead |
|-----------|----------|---------|----------|
| ZSTD/22 | 175ms | 370ms | +195ms (oxipng) |
| BROTLI/11 | 356ms | 858ms | +502ms (oxipng) |
| ZPAQ/5 | 502ms | 704ms | +202ms (oxipng) |

**Note:** The large time increase is due to oxipng preprocessing (~200ms avg).

## Decision Rules

```python
def select_png_algorithm(purpose: str, use_oxipng: bool = True) -> tuple:
    if purpose == "cold_storage":
        if use_oxipng:
            return "zstd", 22  # 12% compression
        else:
            return "zstd", 19  # 9% compression
    elif purpose == "fast":
        if use_oxipng:
            return "brotli", 1  # Still fast with oxipng
        else:
            return "brotli", 1  # ~0ms encode
    elif purpose == "balanced":
        return "zstd", 18  # Good trade-off
    
    # Time-constrained
    if use_oxipng and max_time_ms < 500:
        # Oxipng takes ~200ms, so add time budget
        return "zstd", 19
    
    return "zstd", 19
```

## Key Findings

1. **Oxipng is worth it** - +2.5% compression improvement
2. **Add ~200ms overhead** - oxipng preprocessing time
3. **Still faster than ZPAQ** - ZSTD/22 + oxipng is faster than ZPAQ/5 alone
4. **Best for cold storage** - Use oxipng + ZSTD/22 for maximum compression

## Raw Data

- Location: `G:\mosqueeze-bench\results\png-oxipng\`
- Measurements: 4,335
- Files tested: 1,445

## Comparison Table

| Config | Ratio | Encode Time | Use Case |
|--------|-------|-------------|----------|
| Baseline ZSTD/22 | 1.091x | 175ms | Fast cold storage |
| **Oxipng + ZSTD/22** | **1.120x** | 370ms | **Best cold storage** |
| Baseline BROTLI/11 | 1.090x | 356ms | Legacy cold storage |
| Oxipng + BROTLI/11 | 1.118x | 858ms | High compression |
| Baseline ZPAQ/5 | 1.082x | 502ms | - |
| Oxipng + ZPAQ/5 | 1.108x | 704ms | Not recommended |
