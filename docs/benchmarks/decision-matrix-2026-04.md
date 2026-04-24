# MoSqueeze Decision Matrix — April 2026 (Extended)

Complete benchmark results across 9 corpora (Windows) + code/text corpus (Linux VM), **69,959 total tests**.

## Test Platforms

| Platform | Tests | Notes |
|----------|-------|-------|
| **Windows** | 601 | Full corpora (images, video, audio, archives, documents) |
| **Linux VM** | 69,358 | Code files (Go, TypeScript, C++, Python, Markdown, YAML, etc.) |

## Executive Summary

| Statistic | Value |
|-----------|-------|
| **Total Tests** | 69,959 |
| **File Types** | 69 |
| **Algorithms** | brotli, xz, zstd, zpaq |
| **Compression Levels** | 1-22 (varies by algorithm) |

---

## Skip Recommendation

⛔ **SKIP COMPRESSION** for these file types (already compressed or negative gain):

| Extension | Avg Ratio | Reason |
|-----------|-----------|--------|
| `.7z` | 1.00x | Archive format — already compressed |
| `.gz` | 1.00x | Gzip format — already compressed |
| `.jpg` | 1.01x | JPEG — already lossy compressed |
| `.mov` | 1.00x | QuickTime — container for compressed video |
| `.mp4` | 1.01x | MP4 — container for compressed video |
| `.png` | 1.00x | PNG — already compressed |
| `.wav` | 1.02x | Uncompressed audio — minimal gain |
| `.webp` | 1.01x | WebP — already compressed |
| `.heic` / `.heif` | 1.01x | HEIF — already compressed |
| `.z` | 1.00x | Compressed archive |

---

## Compression Recommendations

### Category: Excellent Compression (5x+ ratio)

Best compression potential — use higher levels for archival.

| Extension | Avg Ratio | Best Ratio | Best Algorithm | Encode Time | Use Case |
|-----------|-----------|------------|----------------|-------------|----------|
| `.json` | 54.98x | 323.69x | xz/9 | 62s | API responses, configs |
| `.txt` | 21.19x | 212.54x | xz/5 | 77ms | Logs, text files |
| `.md` | 13.78x | 14.99x | zstd/9 | 12ms | Documentation |
| `.py` | 88.05x | 260.59x | brotli/11 | 345ms | Python source |
| `.js` | 103.85x | 152.47x | brotli/11 | 140ms | JavaScript |
| `.ts` | 18.24x | 26.98x | brotli/6 | 0ms | TypeScript |
| `.tsx` | 5.18x | 6.68x | brotli/6 | 2ms | React/TSX |
| `.cpp` | 58.86x | 139.72x | zpaq/5 | 461ms | C++ source |
| `.hpp` | 16.47x | 10.45x | xz/3 | 17ms | C++ headers |
| `.h` | 12.82x | 6.95x | brotli/9 | 40ms | C/C++ headers |
| `.go` | 4.32x | 10.98x | xz/3 | 38ms | Go source |
| `.db` (SQLite) | 12.21x | 71.23x | brotli/9 | 60ms | Databases |
| `.sqlite3` | 11.58x | 25.92x | brotli/9 | 4ms | SQLite files |
| `.ninja` | 10.35x | 23.30x | brotli/9 | 14ms | Build files |
| `.cmake` | 5.91x | 8.98x | brotli/9 | 5ms | CMake files |
| `.css` | 4.83x | 2.97x | brotli/6 | 0ms | Stylesheets |
| `.sql` | 4.72x | 3.68x | brotli/6 | 0ms | SQL scripts |

### Category: Good Compression (2-5x ratio)

Good compression with fast encode times.

| Extension | Avg Ratio | Best Ratio | Best Algorithm | Encode Time | Use Case |
|-----------|-----------|------------|----------------|-------------|----------|
| `.yaml` / `.yml` | 4.25x | 8.03x | brotli/9 | 28ms | Config files |
| `.svg` | 3.67x | 3.98x | zstd/9 | 110ms | Vector graphics |
| `.xml` | 18.03x | 45.35x | xz/3 | 12ms | XML data |
| `.html` | 25.98x | 41.25x | brotli/3 | 2ms | Web pages |
| `.log` | 7.86x | 16.36x | brotli/5 | 112ms | Log files |
| `.csv` | 5.54x | 15.96x | brotli/5 | 182ms | CSV data |
| `.c` | 3.21x | 2.88x | brotli/9 | 22ms | C source |
| `.proto` | 3.28x | 4.00x | brotli/9 | 5ms | Protocol buffers |
| `.bin` | 19.53x | 80659x | brotli/3 | 9ms | Binary data |
| `.out` | 9.84x | 27.71x | xz/3 | 12ms | Output files |
| `.a` / `.so` | 4.51x | 10.16x | xz/3 | 404ms | Static/shared libs |

### Category: Moderate Compression (1.5-2x ratio)

Consider compression benefit vs time.

| Extension | Avg Ratio | Best Algorithm | Encode Time | Notes |
|-----------|-----------|----------------|-------------|-------|
| `.list` | 3.91x | brotli/6 | 1ms | File lists |
| `.sample` | 2.73x | brotli/9 | 10ms | Sample files |
| `.mod` | 2.04x | brotli/6 | 0ms | Module files |
| `.sum` | 1.92x | brotli/6 | 1ms | Checksums |
| `.gplv2` / `.0bsd` | 1.89x | brotli/6 | 6ms | License files |
| `.rev` | 2.15x | brotli/6 | 0ms | Revision files |
| `.optimized` | 1.94x | brotli/6 | 1ms | Optimized data |

---

## Algorithm Quick Reference

### Brotli (levels 1-11)

Best for: Web content, text files, fast compression with good ratio.

| Level | Speed | Ratio | Use Case |
|-------|-------|-------|----------|
| 1-3 | Very Fast | 70-80% of max | Real-time, web serving |
| 5-6 | Fast | 85-90% of max | Balanced, frequent access |
| 9-11 | Medium | Max ratio | Archival, cold storage |

### ZSTD (levels 1-22)

Best for: Real-time compression with excellent speed/ratio balance.

| Level | Speed | Ratio | Use Case |
|-------|-------|-------|----------|
| 1-3 | Extremely Fast | 60-70% of max | Real-time, streaming |
| 5-9 | Fast | 80-90% of max | Balanced workloads |
| 15-22 | Slow | Max ratio | Archival |

### XZ (levels 0-9)

Best for: Archival where compression ratio matters more than speed.

| Level | Speed | Ratio | Use Case |
|-------|-------|-------|----------|
| 0-3 | Medium | 70-80% of max | Balanced archival |
| 5-6 | Slow | 90-95% of max | Deep archival |
| 7-9 | Very Slow | Max ratio | Maximum compression |

### ZPAQ (levels 1-5)

Best for: Cold storage, write-once-read-rarely.

⚠️ **Warning**: Very slow decode (50-500ms per file), use only for archival.

| Level | Speed | Ratio | Use Case |
|-------|-------|-------|----------|
| 1-3 | Slow | Good | Rare archival |
| 4-5 | Very Slow | Max ratio | Maximum compression |

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
    
    # SKIP: Already compressed or minimal gain
    SKIP_TYPES = {'.7z', '.gz', '.jpg', '.jpeg', '.mov', '.mp4', '.png', 
                  '.wav', '.webp', '.heic', '.heif', '.z'}
    if ext in SKIP_TYPES:
        return None
    
    # FAST REAL-TIME (< 10ms target)
    FAST_REALTIME = {
        '.json': ('zstd', 3),
        '.js': ('zstd', 3),
        '.ts': ('brotli', 3),
        '.tsx': ('brotli', 3),
        '.html': ('brotli', 3),
        '.xml': ('zstd', 3),
        '.css': ('brotli', 3),
        '.md': ('zstd', 3),
        '.yaml': ('brotli', 3),
        '.yml': ('brotli', 3),
        '.go': ('zstd', 3),
        '.cpp': ('zstd', 3),
        '.hpp': ('zstd', 3),
        '.h': ('brotli', 6),
        '.c': ('brotli', 6),
        '.cmake': ('brotli', 6),
        '.sql': ('brotli', 3),
    }
    if ext in FAST_REALTIME and max_encode_ms and max_encode_ms < 10:
        return FAST_REALTIME[ext]
    
    # BALANCED (good ratio + reasonable time, < 100ms)
    BALANCED = {
        '.json': ('brotli', 9),
        '.txt': ('xz', 5),
        '.ts': ('brotli', 6),
        '.tsx': ('brotli', 6),
        '.cpp': ('brotli', 9),
        '.hpp': ('brotli', 9),
        '.h': ('brotli', 9),
        '.go': ('xz', 3),
        '.py': ('brotli', 5),
        '.log': ('brotli', 5),
        '.csv': ('brotli', 5),
        '.xml': ('zstd', 3),
        '.html': ('brotli', 3),
        '.js': ('zstd', 3),
        '.db': ('brotli', 9),
        '.md': ('zstd', 9),
        '.yaml': ('brotli', 9),
        '.yml': ('brotli', 9),
        '.cmake': ('brotli', 9),
        '.ninja': ('brotli', 9),
        '.sqlite3': ('brotli', 9),
    }
    if ext in BALANCED:
        return BALANCED[ext]
    
    # MAXIMUM COMPRESSION (archival, unlimited time)
    ARCHIVE = {
        '.json': ('xz', 9),
        '.txt': ('xz', 5),
        '.cpp': ('zpaq', 5),
        '.py': ('brotli', 11),
        '.log': ('zpaq', 5),
        '.csv': ('zpaq', 5),  # Warning: very slow
        '.md': ('zstd', 9),
    }
    if max_encode_ms is None:
        if ext in ARCHIVE:
            return ARCHIVE[ext]
    
    # DEFAULT: Balanced
    return ('brotli', 9)
```

---

## Level Recommendations by Use Case

### Real-Time / Web Serving (< 10ms target)

| File Type | Algorithm | Level | Ratio | Encode | Memory |
|-----------|-----------|-------|-------|--------|--------|
| `.json` | zstd | 3 | 164.84x | 41ms | 6MB |
| `.js` | zstd | 3 | 131.35x | 3ms | 6MB |
| `.ts` | brotli | 3 | 18.24x | 0ms | 9MB |
| `.tsx` | brotli | 3 | 5.18x | 0ms | 10MB |
| `.html` | brotli | 3 | 29.45x | 2ms | 5MB |
| `.xml` | zstd | 3 | 34.76x | 3ms | 6MB |
| `.css` | brotli | 3 | 4.83x | 0ms | 9MB |
| `.md` | zstd | 3 | 13.78x | 1ms | 6MB |
| `.go` | zstd | 3 | 4.32x | 1ms | 6MB |
| `.cpp` | zstd | 3 | 54.26x | 3ms | 6MB |
| `.py` | zstd | 3 | 218.38x | 4ms | 6MB |

### Balanced (Fast + Good Ratio, < 100ms)

| File Type | Algorithm | Level | Ratio | Encode | Memory |
|-----------|-----------|-------|-------|--------|--------|
| `.json` | brotli | 9 | 202.42x | 548ms | 28MB |
| `.txt` | xz | 5 | 212.54x | 77ms | 16MB |
| `.ts` | brotli | 6 | 26.98x | 0ms | 10MB |
| `.hpp` | brotli | 9 | 8.35x | 62ms | 18MB |
| `.go` | xz | 3 | 10.98x | 38ms | 16MB |
| `.md` | zstd | 9 | 14.99x | 12ms | 10MB |
| `.yaml` | brotli | 9 | 8.03x | 28ms | 7MB |
| `.cmake` | brotli | 9 | 8.98x | 5ms | 17MB |

### Maximum Compression (Archival)

| File Type | Algorithm | Level | Ratio | Encode |
|-----------|-----------|-------|-------|--------|
| `.json` | xz | 9 | **323.69x** | 62s |
| `.txt` | xz | 5 | **212.54x** | 77ms |
| `.cpp` | zpaq | 5 | **139.72x** | 461ms |
| `.py` | brotli | 11 | **260.59x** | 345ms |
| `.md` | zstd | 9 | **14.99x** | 12ms |

---

## Test Environment

| Platform | OS | RAM | Notes |
|----------|------|-----|-------|
| **Windows** | Windows 10/11 | 16GB+ | Full corpora, all algorithms |
| **Linux VM** | Linux | 900MB | Code corpus, limited algorithms |

---

## Files Generated

| File | Description |
|------|-------------|
| `decision-matrix-2026-04.md` | This document |
| `combined_decision_matrix.json` | Aggregated decision matrix (JSON) |
| `benchmark-vm-results/` | Linux VM benchmark results |

---

## Benchmark Data Summary

### Extensions Tested (69 total)

**Excellent Compression (5x+):** `.json`, `.txt`, `.md`, `.py`, `.js`, `.ts`, `.tsx`, `.cpp`, `.hpp`, `.h`, `.go`, `.db`, `.sqlite3`, `.ninja`, `.cmake`, `.css`, `.sql`

**Good Compression (2-5x):** `.yaml`, `.yml`, `.svg`, `.xml`, `.html`, `.log`, `.csv`, `.c`, `.proto`, `.bin`, `.out`, `.a`, `.so`

**Moderate Compression (1.5-2x):** `.list`, `.sample`, `.mod`, `.sum`, `.gplv2`, `.0bsd`, `.rev`, `.optimized`

**Skip Compression (< 1.5x):** `.7z`, `.gz`, `.jpg`, `.jpeg`, `.mov`, `.mp4`, `.png`, `.wav`, `.webp`, `.heic`, `.heif`, `.z`

---

*Generated: 2026-04-25*

*Total benchmarks: 69,959 (Linux VM: 69,358, Windows: 601)*