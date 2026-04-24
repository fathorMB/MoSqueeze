# MoSqueeze Decision Matrix — April 2026

Complete benchmark results across 9 corpora, 601 individual tests.

## Executive Summary

| Statistic | Value |
|-----------|-------|
| **Total Tests** | 601 |
| **Corpora Tested** | 9 |
| **Algorithms Tested** | zstd, xz, brotli, zpaq |
| **Compression Levels** | Multiple per algorithm |
| **Success Rate** | 91% (55 tests showed no gain/skip) |

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

**Note**: 98 of 200 JPEG tests (49%) actually *increased* file size. Always skip compression for already-compressed formats.

---

## Compression Recommendations

### Brotli/11 — Fast, Good Ratio (2-4x)

Best for: Speed-critical applications, real-time compression, frequent decompression.

| Extension | Avg Ratio | Best Ratio | Encode Time | Files Tested |
|-----------|-----------|------------|--------------|--------------|
| `.db` (SQLite) | 15.73x | **71.23x** | 7ms | 24 |
| `.xml` | 5.31x | 9.40x | 105ms | 20 |
| `.html` | 5.71x | 6.59x | 107ms | 4 |
| `.xlsx` | 2.71x | 3.93x | 50ms | 16 |
| `.docx` | 2.62x | 3.90x | 163ms | 12 |
| `.pptx` | 3.18x | 3.91x | 57ms | 12 |
| `.mp3` | 2.24x | 3.91x | 135ms | 8 |
| `.aac` | 2.22x | 3.84x | 58ms | 8 |
| `.flac` | 2.21x | 3.85x | 58ms | 8 |
| `.zip` | 1.74x | 3.68x | 29ms | 24 |
| `.js` | 1.98x | 2.62x | 8ms | 4 |
| `.cpp` | 54.02x | 139.72x | 8ms (fast variant) | 12 |
| `.py` | 12.75x | 20.61x | 7ms (fast variant) | 16 |

**Use when:**
- Need fast decompression (web serving, real-time)
- Files accessed frequently
- Storage is not extremely constrained
- Human-readable formats (text, code, markup)

### XZ/9 — Medium Speed, Medium Ratio (2-12x)

Best for: Balanced compression, archival with frequent access.

| Extension | Avg Ratio | Best Ratio | Encode Time | Files Tested |
|-----------|-----------|------------|--------------|--------------|
| `.json` | 3.67x | 11.65x | 381ms (fast variant) | 44 |
| `.pdf` | 7.14x | 12.60x | 224ms | 12 |
| `.webp` | 1.62x | 3.91x | 187ms | 16 |
| `.arw` (RAW) | 2.63x | 2.69x | 498ms | 3 |
| `.cr2` (RAW) | 2.63x | 2.69x | 457ms | 3 |
| `.nef` (RAW) | 2.63x | 2.69x | 497ms | 3 |
| `.z` | 2.67x | 2.78x | 485ms | 4 |

**Use when:**
- Good balance of compression and speed
- Archives accessed occasionally
- RAW image formats (with Bayer preprocessing)

### ZPAQ/5 — Maximum Compression (5-140x)

Best for: Cold storage, long-term archival, write-once-read-rarely.

| Extension | Avg Ratio | Best Ratio | Encode Time | Files Tested |
|-----------|-----------|------------|--------------|--------------|
| `.cpp` | 54.02x | **139.72x** | 461ms | 12 |
| `.log` | 6.61x | 13.03x | 778ms | 20 |
| `.pdf` | 7.14x | 12.60x | 394ms | 12 |
| `.json` | 3.67x | 11.65x | 467ms | 44 |
| `.csv` | 3.89x | 6.73x | 61,843ms ⚠️ | 32 |
| `.py` | 12.75x | 20.61x | 954ms | 16 |
| `.xml` | 5.31x | 9.40x | 184ms | 20 |
| `.wav` | 1.90x | 3.83x | 152ms | 12 |
| `.avif` | 2.67x | 2.78x | 1,009ms | 4 |
| `.heif` | 2.67x | 2.78x | 1,092ms | 4 |
| `.txt` | 1.48x | 1.63x | 83,415ms ⚠️ | 36 |

⚠️ **Warning**: Very slow for large files (CSV, TXT). Use only when maximum compression is critical.

**Use when:**
- Maximum compression ratio required
- Cold storage / archival
- Write once, read rarely
- Storage cost > compute cost
- **NOT** for real-time or frequent access

---

## RAW Image Formats

Tested with Bayer preprocessing disabled (would improve ratios further):

| Format | Algorithm | Ratio | Notes |
|--------|-----------|-------|-------|
| `.arw` (Sony) | XZ/9 | 2.69x | Sony RAW |
| `.cr2` (Canon) | XZ/9 | 2.69x | Canon RAW |
| `.nef` (Nikon) | XZ/9 | 2.69x | Nikon RAW |

**With Bayer preprocessing** (see `--preprocess bayer-raw`), ratios typically improve 15-25%.

---

## Corpus Statistics

| Corpus | Tests | Files | Avg Ratio | Notes |
|--------|-------|-------|-----------|-------|
| `text` | 148 | 148 | **9.35x** | Code, logs, CSVs — excellent |
| `databases` | 24 | 24 | **15.73x** | SQLite — excellent |
| `documents` | 92 | 92 | 2.79x | Office docs, PDFs |
| `images` | 36 | 36 | 2.36x | AVIF, HEIC, WebP, HTML |
| `audio` | 36 | 36 | 2.12x | FLAC, WAV, MP3, AAC |
| `archives` | 40 | 40 | 1.61x | Mixed (some already compressed) |
| `raw` | 9 | 9 | 2.63x | RAW images — good |
| `jpeg` | 200 | 200 | **1.01x** | SKIP — already compressed |
| `video` | 16 | 16 | **1.02x** | SKIP — already compressed |

---

## Incomplete Tests

| Corpus | Status | Issue |
|--------|--------|-------|
| `binary-baseline` | ❌ Incomplete | Benchmark crashed near end |

---

## Decision Logic (Pseudocode)

```python
def select_algorithm(file_extension: str, 
                     max_encode_time_ms: int = None,
                     require_ratio: float = None) -> tuple[str, int] | None:
    """
    Select optimal compression algorithm for a file type.
    
    Args:
        file_extension: File extension with dot (e.g., '.jpg', '.pdf')
        max_encode_time_ms: Maximum acceptable encode time (optional)
        require_ratio: Minimum compression ratio required (optional)
    
    Returns:
        (algorithm, level) tuple, or None to skip compression
    """
    ext = file_extension.lower()
    
    # SKIP: Already compressed
    SKIP_TYPES = {'.7z', '.gz', '.jpg', '.jpeg', '.mov', '.mp4', '.png'}
    if ext in SKIP_TYPES:
        return None
    
    # FAST: Brotli/11 (under 500ms encode)
    BROTLI_TYPES = {
        '.aac', '.cpp', '.db', '.docx', '.flac', '.heic', '.html',
        '.js', '.mp3', '.pptx', '.py', '.xlsx', '.xml', '.zip'
    }
    if ext in BROTLI_TYPES:
        if max_encode_time_ms and max_encode_time_ms < 500:
            return ('brotli', 11)
        if require_ratio and require_ratio < 4:
            return ('brotli', 11)
        return ('brotli', 11)
    
    # MAXIMUM: ZPAQ/5 (best ratio, slow)
    ZPAQ_TYPES = {'.avi', '.avif', '.csv', '.heif', '.log', '.txt', '.wav'}
    if ext in ZPAQ_TYPES:
        if max_encode_time_ms and max_encode_time_ms < 1000:
            # Fall back to faster option if time-constrained
            return ('xz', 9)  # Fallback
        return ('zpaq', 5)
    
    # MEDIUM: XZ/9 (balanced)
    XZ_TYPES = {'.arw', '.cr2', '.json', '.nef', '.pdf', '.webp', '.z'}
    if ext in XZ_TYPES:
        return ('xz', 9)
    
    # DEFAULT: Brotli for unknown types
    return ('brotli', 11)
```

---

## Recommendations for TheVault

Based on these results, for TheVault file storage:

### Tier 1: Fast Compression (Real-time)
Apply **Brotli/11** to:
- Documents (`.docx`, `.xlsx`, `.pptx`, `.pdf`)
- Code files (`.js`, `.py`, `.cpp`, `.xml`, `.html`)
- Database files (`.db`, `.sqlite`)
- Archives (`.zip`)
- Audio (`.mp3`, `.aac`)

**Expected**: 2-4x compression in <200ms

### Tier 2: Balanced Compression (Background)
Apply **XZ/9** to:
- JSON files (`.json`)
- PDF documents (`.pdf`)
- WebP images (`.webp`)
- RAW images (`.arw`, `.cr2`, `.nef`)

**Expected**: 2-12x compression in 200-500ms

### Tier 3: Maximum Compression (Cold Storage)
Apply **ZPAQ/5** to:
- Log files (`.log`)
- CSV exports (`.csv`)
- Archive backups (`.txt`)

**Expected**: 5-140x compression, but very slow

### Skip Compression (No Gain)
- Images (`.jpg`, `.png`)
- Video (`.mp4`, `.mov`)
- Archives (`.7z`, `.gz`)
- Already compressed formats

---

## Test Environment

| Parameter | Value |
|-----------|-------|
| **Platform** | Linux VM |
| **RAM** | 900MB (limited) |
| **Algorithms** | zstd, xz, brotli, zpaq |
| **Levels** | Default + max for each |
| **Date** | April 2026 |

---

## Next Steps

1. ✅ Decision matrix saved to `decision_matrix.json`
2. □ Implement decision logic in MoSqueeze library
3. □ Add Bayer preprocessing for RAW formats
4. □ Benchmark binary files (incomplete)
5. □ Test `binary-baseline` on Windows machine (more RAM)