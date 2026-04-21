# Benchmark Results Index

**Summary**: Indice dei risultati storici dei benchmark.

**Last updated**: 2026-04-22

---

## Latest Results

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

| Date | Corpus Size | Files | Algorithms | Notes |
|------|-------------|-------|------------|-------|
| 2026-04-22 | 50 MB | 12 | zstd, lzma, brotli | Initial benchmark |

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