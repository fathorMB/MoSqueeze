# PNG Baseline Benchmark Results

**Date:** 2026-04-23
**Corpus:** 1,445 PNG files (0.2 KB - 2.5 MB, median 78 KB)
**Run:** Default levels only (ZSTD/22, XZ/9, BROTLI/11, ZPAQ/5)

## Summary

| Algorithm | Level | Avg Ratio | Avg Encode | Files Tested |
|-----------|-------|-----------|------------|--------------|
| **ZSTD** | 22 | **1.0914x** | 175ms | 1,445 |
| BROTLI | 11 | 1.0901x | 356ms | 1,445 |
| XZ | 9 | 1.0849x | 50ms | 1,445 |
| ZPAQ | 5 | 1.0819x | 502ms | 1,445 |

## Key Findings

### 1. PNG Files Are Compressible!

Contrary to expectations, PNG files show **~9% compression potential**:
- **92.3%** of files are compressible (ratio ≥ 1.01)
- Only **7.7%** are already optimized (ratio < 1.01)

This is because PNG uses DEFLATE (same as gzip) which modern algorithms can improve upon.

### 2. Best Algorithm Depends on Use Case

| Use Case | Recommended | Why |
|----------|-------------|-----|
| **Cold storage** | ZSTD/22 | Best ratio (9.1%), reasonable time |
| **Fast compression** | XZ/9 | Only 50ms, ~8.5% ratio |
| **Balance** | ZSTD/19-20 | Good ratio, faster than /22 |

### 3. ZPAQ Underperforms on PNG

On RAW files, ZPAQ/5 achieved **2.35%** improvement.
On PNG, only **0.82%** - PNG's DEFLATE already removes most redundancy.

### 4. File Size Distribution

```
Min:    0.2 KB
Max:    2,540 KB (2.5 MB)
Avg:    186 KB
Median: 78 KB
```

Most files are small-to-medium, making encode time critical.

## Decision Rules (Draft)

```python
# For PNG files
if purpose == "cold_storage":
    return "zstd", 22  # Best ratio
elif purpose == "fast":
    return "xz", 9     # 50ms, good ratio
elif purpose == "balanced":
    return "zstd", 19   # Good tradeoff
```

## Next Steps

- [x] PNG baseline (default levels)
- [ ] PNG full matrix (all levels) - RUNNING
- [ ] PNG with oxipng preprocessing
- [ ] JPEG baseline
- [ ] Video formats

## Raw Data

- Location: `G:\mosqueeze-bench\results\png-baseline\`
- Format: CSV, JSON, SQLite3
- Files: 1,445 PNG × 4 algorithms = 5,780 measurements
