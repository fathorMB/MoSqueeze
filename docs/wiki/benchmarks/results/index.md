# Benchmark Results Index

**Summary**: Indice dei risultati storici dei benchmark.

**Last updated**: 2026-04-23

---

## Latest Results

### 2026-04-23 — PNG Benchmark (Extended)

**Commit**: `feat/compressed-formats-corpus`
**Environment**:
- OS: Windows 11
- CPU: AMD Ryzen 9 5900X
- Memory: 64 GB DDR4
- Compiler: MSVC 2022

**Corpus**: 1,445 PNG files

**Results**:
- [PNG Baseline Results](../../benchmarks/png-full-matrix-results.md) — 70,805 measurements
- [PNG + Oxipng Results](../../benchmarks/png-oxipng-results.md) — 4,335 measurements with preprocessing

**Highlights**:
- **PNG is compressible**: 9-12% compression ratio
- **ZSTD/19 sweet spot**: Same ratio as ZSTD/22, 4x faster
- **Oxipng preprocessing**: +2.5% compression improvement
- **Best config**: Oxipng + ZSTD/22 = 1.120x ratio (370ms)

### 2026-04-22 — Initial Benchmark

**Commit**: `aa48401`
**Environment**:
- OS: Ubuntu 22.04 (WSL)
- CPU: AMD Ryzen 9 5900X
- Memory: 64 GB DDR4
- Compiler: GCC 12.2

**Files**:
- `results/2026-04-22/summary.json`
- `results/2026-04-22/details.csv`
- `results/2026-04-22/graphs/`

**Highlights**:
- brotli-11 best ratio on JSON (16.8x)
- lzma-9 best ratio on binary (2.8x)
- zstd-22 best balance overall

---

## Results Archive

| Date | Corpus | Files | Measurements | Notes |
|------|--------|-------|--------------|-------|
| 2026-04-23 | PNG Corpus | 1,445 | 75,140 | PNG baseline + full + oxipng |
| 2026-04-22 | Text/Binary | 12 | TBD | Initial benchmark |

---

## PNG Results Summary

### Baseline vs Oxipng Preprocessing

| Algorithm | Baseline | + Oxipng | Improvement |
|-----------|----------|----------|-------------|
| ZSTD/22 | 1.091x | **1.120x** | **+2.65%** |
| BROTLI/11 | 1.090x | 1.118x | +2.60% |
| ZPAQ/5 | 1.082x | 1.108x | +2.45% |

### Key Findings

1. **PNG is compressible** — Contrary to common belief, modern algorithms can improve upon PNG's embedded DEFLATE
2. **ZSTD/19 is the sweet spot** — Same compression as ZSTD/22, but 4x faster (42ms vs 180ms)
3. **Oxipng preprocessing helps** — +2.5% compression by stripping metadata and optimizing filters
4. **Best cold storage config** — Oxipng + ZSTD/22 achieves 1.120x ratio

---

## How to Add Results

Dopo ogni benchmark run significativa:

1. Salva risultati con timestamp
```
results/YYYY-MM-DD/
├── summary.json
├── details.csv
├── environment.json
└── graphs/
    ├── ratio-by-algorithm.png
    └── time-vs-ratio.png
```

2. Aggiorna questo index
3. Genera wiki pages per findings notevoli

---

## Querying Results

### SQLite Query Examples

```sql
-- Best ratio per algorithm
SELECT algorithm, level, AVG(originalBytes / compressedBytes) as avgRatio
FROM benchmark_results
GROUP BY algorithm, level
ORDER BY avgRatio DESC;

-- Best for text files
SELECT algorithm, level, AVG(originalBytes / compressedBytes) as avgRatio
FROM benchmark_results
WHERE fileType LIKE 'Text_%'
GROUP BY algorithm, level
ORDER BY avgRatio DESC;

-- Fastest encode (throughput)
SELECT algorithm, level, 
       SUM(originalBytes) / SUM(encodeTimeMs) as throughput_MBps
FROM benchmark_results
GROUP BY algorithm, level
ORDER BY throughput DESC;

-- PNG performance with oxipng
SELECT algorithm, level, AVG(ratio) as avg_ratio, AVG(encode_ms) as avg_time
FROM png_oxipng_results
GROUP BY algorithm, level
ORDER BY avg_ratio DESC;
```

---

## Trend Analysis

### Comparative Queries

```sql
-- Compare two runs
SELECT 
    r1.algorithm, r1.level,
    r1.ratio as ratio_v1,
    r2.ratio as ratio_v2,
    (r2.ratio - r1.ratio) / r1.ratio * 100 as percent_change
FROM benchmark_results r1
JOIN benchmark_results r2 ON r1.file = r2.file AND r1.algorithm = r2.algorithm
WHERE r1.timestamp = '2026-04-22'
  AND r2.timestamp = '2026-05-01';
```

### Regression Detection

Se ratio peggiora >2% tra run:
1. Indagare cambiamenti codice
2. Controllare versione librerie
3. Verificare environment

---

## Related Pages

- [[methodology]] — Come eseguire benchmark
- [[corpus-selection]] — File di test
- [[graphs/ratio-by-algorithm]] — Grafici comparativi
- [[../../benchmarks/png-full-matrix-results]] — PNG full matrix data
- [[../../benchmarks/png-oxipng-results]] — PNG + oxipng results
