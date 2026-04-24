# MoSqueeze Decision Matrix — April 2026 (Extended)

Complete benchmark results across 9 corpora (Windows) + synthetic tests (Linux VM), **753 total tests**.

## Test Platforms

| Platform | Tests | Notes |
|----------|-------|-------|
| **Windows** | 601 | Full corpora (images, video, audio, archives, documents) |
| **Linux VM** | 152 | Synthetic files + code (900MB RAM limit) |

## Executive Summary

| Statistic | Value |
|-----------|-------|
| **Total Tests** | 753 |
| **File Types** | 34 |
| **Algorithms** | brotli, xz, zstd, zpaq |
| **Compression Levels** | 3-22 (varies by algorithm) |

---

## Skip Recommendation

⛔ **SKIP COMPRESSION** for these file types (already compressed or negative gain):

| Extension | Avg Ratio | Reason |
|-----------|-----------|--------|
| `.7z` | 1.00x | Archive format — already compressed |
| `.gz` | 1.00x | Gzip format — already compressed |
| `.jpg` | 1.01x | JPEG — already lossy compressed (49% get LARGER) |
| `.mov` | 1.00x | QuickTime — container for compressed video |
| `.mp4` | 1.01x | MP4 — container for compressed video |
| `.png` | 1.00x | PNG — already compressed |

---

## Compression Recommendations

### Brotli/5-11 — Fast, Good Ratio (2-4x typical)

Best for: Speed-critical applications, real-time compression, frequent access.

| Extension | Avg Ratio | Best Ratio | Encode Time | Files Tested |
|-----------|-----------|------------|--------------|--------------|
| `.db` (SQLite) | 12.21x | **71.23x** | 7-60ms | 30 |
| `.json` | 54.98x | **323.69x** | 5-7535ms | 66 |
| `.py` | 88.05x | **260.59x** | 19-345ms | 24 |
| `.js` | 103.85x | **152.47x** | 5-140ms | 18 |
| `.cpp` | 58.86x | **139.72x** | 6-2356ms | 26 |
| `.xml` | 18.03x | **45.35x** | 4-629ms | 34 |
| `.html` | 25.98x | **41.25x** | 2-491ms | 18 |
| `.txt` | 21.19x | **212.54x** | 20-4856ms | 44 |
| `.log` | 7.86x | **16.36x** | 112-6924ms | 28 |
| `.csv` | 5.54x | **15.96x** | 182-8254ms | 40 |
| `.xlsx` | 2.71x | 3.93x | 50ms | 16 |
| `.docx` | 2.62x | 3.90x | 163ms | 12 |
| `.pptx` | 3.18x | 3.91x | 57ms | 12 |
| `.mp3` | 2.24x | 3.91x | 135ms | 8 |
| `.aac` | 2.22x | 3.84x | 58ms | 8 |
| `.flac` | 2.21x | 3.85x | 58ms | 8 |
| `.zip` | 1.74x | 3.68x | 29ms | 24 |

**Linux VM Specific (brotli):**
| Extension | Best Level | Ratio | Encode | Memory |
|-----------|------------|-------|--------|--------|
| `.json` | 11 | 259.43x | 7535ms | 90MB |
| `.json` | 9 | 202.42x | 548ms | 28MB ⭐ |
| `.json` | 5 | 181.25x | 348ms | 16MB |
| `.cpp` | 9 | 61.51x | 26ms | 18MB ⭐ |
| `.py` | 11 | 260.59x | 345ms | 28MB |
| `.js` | 11 | 145.25x | 140ms | 15MB |

### XZ/5-9 — Medium Speed, High Ratio (2-12x typical)

Best for: Balanced compression, archival with occasional access.

| Extension | Avg Ratio | Best Ratio | Encode Time | Files Tested |
|-----------|-----------|------------|--------------|--------------|
| `.json` | 54.98x | **323.69x** | 62386ms (level 9) | 66 |
| `.txt` | 21.19x | **212.54x** | 77ms (level 5) | 44 |
| `.cpp` | 58.86x | **86.92x** | 735ms | 26 |
| `.xml` | 18.03x | **45.35x** | 12ms (level 3) | 34 |
| `.html` | 25.98x | **41.25x** | 124ms | 18 |
| `.pdf` | 7.14x | **12.60x** | 394ms | 12 |
| `.log` | 7.86x | **16.36x** | 4046ms | 28 |
| `.arw` (RAW) | 2.63x | 2.69x | 498ms | 3 |
| `.cr2` (RAW) | 2.63x | 2.69x | 457ms | 3 |
| `.nef` (RAW) | 2.63x | 2.69x | 497ms | 3 |

**Linux VM Specific (xz):**
| Extension | Best Level | Ratio | Encode | Memory |
|-----------|------------|-------|--------|--------|
| `.json` | 9 | 323.69x | 62386ms | 90MB ⚠️ SLOW |
| `.json` | 5 | 290.47x | 1198ms | 16MB |
| `.txt` | 5 | 212.54x | 77ms | 16MB ⭐ |
| `.cpp` | 9 | 86.92x | 735ms | 15MB |
| `.xml` | 3 | 45.35x | 12ms | 15MB ⭐ |

### ZSTD/3-19 — Fast, Good Balance (2-50x typical)

Best for: Real-time compression with good ratio, frequent reads.

| Extension | Best Level | Ratio | Encode | Notes |
|-----------|------------|-------|--------|-------|
| `.json` | 3 | 164.84x | 41ms | ⭐ Fastest |
| `.py` | 9 | 228.39x | 28ms | ⭐ Best balance |
| `.cpp` | 5 | 56.18x | 7ms | ⭐ Fast |
| `.xml` | 3 | 34.76x | 3ms | ⭐ Fastest |
| `.csv` | 3 | 10.90x | 41ms | ⭐ Best for CSV |

### ZPAQ/5 — Maximum Compression (5-140x)

Best for: Cold storage, write-once-read-rarely, maximum compression needed.

⚠️ **Warning**: Very slow encode (500ms-80s), use only for archival.

| Extension | Ratio | Encode Time | Notes |
|-----------|-------|-------------|-------|
| `.cpp` | **139.72x** | 461ms | Code source |
| `.py` | **20.61x** | 954ms | Python |
| `.log` | **13.03x** | 778ms | Log files |
| `.pdf` | **12.60x** | 394ms | PDFs |
| `.json` | **11.65x** | 467ms | JSON |
| `.csv` | **6.73x** | 61843ms ⚠️ | Very slow for large CSV |

---

## Level Recommendations by Use Case

### Real-Time / Web Serving (< 50ms target)

| File Type | Algorithm | Level | Ratio | Encode | Memory |
|-----------|-----------|-------|-------|--------|--------|
| `.json` | zstd | 3 | 164.84x | 41ms | 6MB |
| `.js` | zstd | 3 | 131.35x | 3ms | 6MB |
| `.html` | brotli | 3 | 29.45x | 2ms | 5MB |
| `.xml` | zstd | 3 | 34.76x | 3ms | 6MB |
| `.cpp` | zstd | 3 | 54.26x | 3ms | 6MB |
| `.py` | zstd | 3 | 218.38x | 4ms | 6MB |

### Balanced (Fast + Good Ratio)

| File Type | Algorithm | Level | Ratio | Encode | Memory |
|-----------|-----------|-------|-------|--------|--------|
| `.json` | brotli | 9 | 202.42x | 548ms | 28MB |
| `.txt` | xz | 5 | 212.54x | 77ms | 16MB |
| `.cpp` | brotli | 9 | 61.51x | 26ms | 18MB |
| `.py` | brotli | 5 | 241.43x | 19ms | 16MB |
| `.log` | brotli | 5 | 9.57x | 112ms | 16MB |
| `.csv` | brotli | 5 | 11.35x | 182ms | 16MB |

### Maximum Compression (Archival)

| File Type | Algorithm | Level | Ratio | Encode |
|-----------|-----------|-------|-------|--------|
| `.json` | xz | 9 | **323.69x** | 62s |
| `.txt` | xz | 5 | **212.54x** | 77ms |
| `.cpp` | zpaq | 5 | **139.72x** | 461ms |
| `.py` | brotli | 11 | **260.59x** | 345ms |

---

## RAW Image Formats

Tested with Bayer preprocessing disabled. With preprocessing, expect +15-25% improvement.

| Format | Algorithm | Level | Ratio | Notes |
|--------|-----------|-------|-------|-------|
| `.arw` (Sony) | xz | 9 | 2.69x | Sony RAW |
| `.cr2` (Canon) | xz | 9 | 2.69x | Canon RAW |
| `.nef` (Nikon) | xz | 9 | 2.69x | Nikon RAW |

---

## Decision Logic

```python
def select_algorithm(extension: str, 
                     max_encode_ms: int = None,
                     min_ratio: float = None) -> tuple[str, int] | None:
    """
    Select optimal compression based on benchmark data.
    
    Returns: (algorithm, level) or None to skip compression
    """
    ext = extension.lower()
    
    # SKIP: Already compressed
    SKIP_TYPES = {'.7z', '.gz', '.jpg', '.jpeg', '.mov', '.mp4', '.png'}
    if ext in SKIP_TYPES:
        return None
    
    # REAL-TIME (< 50ms target)
    REALTIME_TYPES = {'.json', '.js', '.html', '.xml', '.cpp', '.py'}
    if ext in REALTIME_TYPES and max_encode_ms and max_encode_ms < 50:
        return ('zstd', 3)
    
    # BALANCED (good ratio + reasonable time)
    BALANCED_TYPES = {
        '.json': ('brotli', 9),
        '.txt': ('xz', 5),
        '.cpp': ('brotli', 9),
        '.py': ('brotli', 5),
        '.log': ('brotli', 5),
        '.csv': ('brotli', 5),
        '.xml': ('zstd', 3),
        '.html': ('brotli', 3),
        '.js': ('zstd', 3),
        '.db': ('brotli', 9),
    }
    if ext in BALANCED_TYPES:
        return BALANCED_TYPES[ext]
    
    # MAXIMUM COMPRESSION (archival)
    ARCHIVE_TYPES = {
        '.log': ('zpaq', 5),
        '.csv': ('zpaq', 5),  # Warning: very slow
    }
    if max_encode_ms is None and min_ratio and min_ratio >= 10:
        if ext in ARCHIVE_TYPES:
            return ARCHIVE_TYPES[ext]
    
    # DEFAULT: Balanced
    return ('brotli', 9)
```

---

## Test Environment

| Platform | OS | RAM | Notes |
|----------|------|-----|-------|
| **Windows** | Windows 10/11 | 16GB+ | Full corpora, all algorithms |
| **Linux VM** | Linux | 900MB | Synthetic tests, limited algorithms |

---

## Files Generated

| File | Description |
|------|-------------|
| `decision-matrix-2026-04.md` | This document |
| `decision_runs/` | Windows corpus results (9 directories) |
| `linux_vm_results/benchmark_results.json` | Linux VM raw results |
| `combined_decision_matrix.json` | Aggregated decision matrix |

---

## Next Steps

1. ✅ 753 tests completed across Windows + Linux VM
2. ✅ Decision logic implemented
3. □ Bayer preprocessing for RAW formats
4. □ Test on real TheVault workload
5. □ Implement adaptive compression in MoSqueeze library