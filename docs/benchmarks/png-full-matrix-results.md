# PNG Benchmark Results - Complete Matrix

**Date:** 2026-04-23
**Corpus:** 1,445 PNG files (0.2 KB - 2.5 MB, median 78 KB)
**Measurements:** 70,805 total (1,445 files × 49 algorithm-level combinations)

## Executive Summary

PNG files are **~9% compressible** with modern algorithms. ZSTD/19 provides **near-optimal compression** with **excellent speed**.

| Use Case | Algorithm | Level | Ratio | Time | Recommendation |
|----------|-----------|-------|-------|------|----------------|
| **Cold Storage** | ZSTD | 19 | 1.091x | 42ms | Best ratio/speed |
| **Fast** | BROTLI | 1 | 1.057x | ~0ms | Instant |
| **Balanced** | ZSTD | 18 | 1.091x | 26ms | Sweet spot |
| Legacy | XZ | 6 | 1.085x | 34ms | Compatible |

---

## Full Results Matrix

### ZSTD (22 levels)

| Level | Ratio | Encode | Decode | Score |
|-------|-------|--------|--------|-------|
| 1 | 1.058x | 0ms | 0ms | 942 |
| 3 | 1.073x | 0ms | 0ms | 800 |
| 5 | 1.077x | 1ms | 0ms | 468 |
| 10 | 1.078x | 6ms | 0ms | 180 |
| 17 | 1.087x | 24ms | 0ms | 44 |
| **18** | **1.091x** | **26ms** | **0ms** | **41** |
| **19** | **1.091x** | **42ms** | **0ms** | **25** |
| 22 | 1.091x | 180ms | 0ms | 6 |

**Key insight:** ZSTD/19 = ZSTD/22 ratio with **4x less time**.

### BROTLI (12 levels)

| Level | Ratio | Encode | Decode | Score |
|-------|-------|--------|--------|-------|
| 0 | 1.049x | 0ms | 0ms | 985 |
| 1 | 1.057x | 0ms | 0ms | 995 |
| 5 | 1.074x | 1ms | 0ms | 468 |
| 9 | 1.087x | 16ms | 0ms | 64 |
| 11 | 1.090x | 356ms | 0ms | 3 |

**Key insight:** BROTLI/1 is **instant** with 5.7% compression.

### XZ (10 levels)

| Level | Ratio | Encode | Decode |
|-------|-------|--------|--------|
| 0 | 1.049x | 49ms | 4ms |
| 6 | 1.085x | 34ms | 4ms |
| 9 | 1.085x | 50ms | 4ms |

**Key insight:** XZ is consistent across levels, good for compatibility.

### ZPAQ (5 levels)

| Level | Ratio | Encode | Decode |
|-------|-------|--------|--------|
| 1 | 1.062x | 8ms | 9ms |
| 5 | 1.082x | 510ms | 119ms |

**Key insight:** ZPAQ **underperforms** on PNG compared to ZSTD.

---

## Decision Rules

```python
# decision_rules.py
# For PNG files (1,445 files tested)

def select_algorithm(purpose: str, max_time_ms: int = None) -> tuple:
    """
    Select optimal compression for PNG files.
    
    Based on benchmark of 1,445 PNG files (70,805 measurements).
    
    Args:
        purpose: "cold_storage", "fast", "balanced", "legacy"
        max_time_ms: Optional time constraint
    
    Returns:
        (algorithm: str, level: int)
    """
    
    if purpose == "cold_storage":
        # Best ratio, acceptable time
        return "zstd", 19  # 1.091x in 42ms
    
    elif purpose == "fast":
        # Instant compression
        return "brotli", 1  # 1.057x in ~0ms
    
    elif purpose == "balanced":
        # Best ratio/time tradeoff
        return "zstd", 18  # 1.091x in 26ms
    
    elif purpose == "legacy":
        # Maximum compatibility
        return "xz", 6  # 1.085x in 34ms
    
    # Time-constrained selection
    if max_time_ms is not None:
        if max_time_ms < 5:
            return "brotli", 1
        elif max_time_ms < 30:
            return "zstd", 18
        elif max_time_ms < 50:
            return "zstd", 19
        else:
            return "zstd", 22
    
    return "zstd", 19  # Default
```

---

## Key Findings

### 1. PNG IS Compressible! (~9%)

Contrary to common belief, PNG files show substantial compression potential:
- **92.3%** of files compress >1%
- **~9%** average ratio improvement
- PNG uses DEFLATE; modern algorithms improve upon it

### 2. ZSTD/19 is the Sweet Spot

| Level | Ratio Gain vs /22 | Time Savings |
|-------|-------------------|--------------|
| 19 | 0.01% | 77% faster |
| 22 | baseline | - |

ZSTD/22 provides **negligible ratio improvement** over /19 with **4x the time**.

### 3. BROTLI for Instant Compression

Level 0-1 is essentially free (~0ms) with meaningful compression (5-6%).

### 4. ZPAQ Underperforms on PNG

On RAW files: ZPAQ/5 = 2.35% improvement
On PNG files: ZPAQ/5 = 8.2% improvement (**worse than ZSTD**)

PNG's DEFLATE already removes redundancy that ZPAQ would exploit.

---

## Compression Efficiency

Ratio per millisecond of encode time:

| Algorithm | Level | Efficiency Score |
|-----------|-------|------------------|
| BROTLI | 1 | 995 |
| ZSTD | 1 | 942 |
| ZSTD | 18 | 41 |
| ZSTD | 19 | 25 |
| ZSTD | 22 | 6 |

---

## Next Steps

- [x] PNG baseline (default levels)
- [x] PNG full matrix (all levels) ✅
- [ ] PNG with oxipng preprocessing - **NEXT**
- [ ] JPEG benchmark
- [ ] Video formats

## Raw Data

- Location: `G:\mosqueeze-bench\results\png-full\`
- Size: 22 MB JSON, 7 MB CSV, 8 MB SQLite3
- Measurements: 70,805
