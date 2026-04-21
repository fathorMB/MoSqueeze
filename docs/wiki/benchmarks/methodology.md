# Benchmark Methodology

**Summary**: Come vengono eseguiti i benchmark in MoSqueeze.

**Last updated**: 2026-04-22

---

## Overview

MoSqueeze usa un benchmark harness per generare dati comparativi reali tra algoritmi.

### Tool

```bash
mosqueeze-bench --corpus corpus/ --output results/
```

---

## Misurazioni

### Metriche Primary

| Metric | Unit | Description |
|--------|------|-------------|
| **Ratio** | factor | `originalBytes / compressedBytes` |
| **Encode Time** | ms | Tempo di compressione |
| **Decode Time** | ms | Tempo di decompressione |
| **Peak Memory** | bytes | Memoria massima usata |

### Metriche Derivate

- **Throughput**: `originalBytes / encodeTime` (MB/s)
- **Space Savings**: `(1 - 1/ratio) * 100` (%)

---

## Corpus

### Standard Corpus Structure

```
corpus/
├── text/
│   ├── code/         # .cpp, .py, .js, .ts, .rs
│   ├── json/         # Large JSON datasets
│   ├── xml/          # RSS, SOAP, configs
│   ├── logs/         # Server logs, app logs
│   └── markdown/     # README, docs
├── binary/
│   ├── exec/         # ELF, PE executables
│   ├── database/     # SQLite, MDB
│   └── data/         # Custom binary formats
├── image/
│   ├── png/          # Unoptimized PNGs
│   └── raw/          # BMP, TIFF
└── archive/
    └── tar/          # Plain tar files
```

### Selection Criteria

1. **Rappresentatività** — File tipici per caso d'uso reale
2. **Dimensione variabile** — Da KB a GB
3. **Content diversity** — Testo, binario, mixed
4. **Openness** — Nessun copyright issue

### Files Correnti

```
# Generati o scaricati per test
corpus/text/code/
├── large.cpp         # 2.1 MB C++ source
├── bundle.js         # 5.8 MB minified JS
└── kernel_sources/   # 15 MB Linux kernel subset

corpus/text/json/
├── large_dataset.json  # 5.2 MB JSON
└── twitter_sample.json # 12 MB tweets

corpus/binary/exec/
├── test_binary.exe    # 8 MB
└── libsomething.so    # 4 MB
```

---

## Execution

### Grid Search

Per ogni file, testiamo tutti gli algoritmi × tutti i livelli:

```python
algorithms = ['zstd', 'lzma', 'brotli']
levels = {
    'zstd': [3, 10, 19, 22],
    'lzma': [3, 6, 9],
    'brotli': [6, 9, 11]
}

for file in corpus:
    for algo in algorithms:
        for level in levels[algo]:
            run_benchmark(file, algo, level)
```

### Controls

- **Cold runs**: 3x per stabilità statistiche
- **Memory isolation**: Ogni run fresh process
- **CPU isolation**: Affinity pinning quando possibile
- **Timing**: `std::chrono::steady_clock` high resolution

---

## Results Storage

### SQLite Schema

```sql
CREATE TABLE benchmark_results (
    id INTEGER PRIMARY KEY,
    timestamp TEXT,
    algorithm TEXT,
    level INTEGER,
    file_path TEXT,
    file_type TEXT,
    original_bytes INTEGER,
    compressed_bytes INTEGER,
    encode_time_ms INTEGER,
    decode_time_ms INTEGER,
    peak_memory_bytes INTEGER
);
```

### JSON Export

```json
{
  "timestamp": "2026-04-22T15:30:00Z",
  "results": [
    {
      "algorithm": "zstd",
      "level": 19,
      "file": "corpus/text/code/large.cpp",
      "fileType": "Text_SourceCode",
      "originalBytes": 2100000,
      "compressedBytes": 540000,
      "encodeTimeMs": 1200,
      "decodeTimeMs": 45,
      "peakMemoryBytes": 134000000
    }
  ]
}
```

---

## Reproducibility

### Hash Verification

```bash
# Calcolato pre-compressione
sha256sum corpus/file.txt > corpus/file.txt.sha256
```

### Environment Recording

```json
{
  "environment": {
    "os": "Ubuntu 22.04",
    "cpu": "AMD Ryzen 9 5900X",
    "memory": "64 GB DDR4",
    "compiler": "GCC 12.2",
    "zstd_version": "1.5.5",
    "lzma_version": "5.4.1",
    "brotli_version": "1.0.9"
  }
}
```

---

## Analysis

### Reporting

Dopo ogni run:
1. Export JSON + CSV
2. Generate graphs (scripts/generate_graphs.py)
3. Update wiki con risultati notevoli

### Comparison

Si possono confrontare run diverse:
- Tra versioni MoSqueeze
- Tra hardware diversi
- Tempo evoluzione (regression detection)

---

## Related Pages

- [[corpus-selection]] — Dettagli sui file di test
- [[graphs/ratio-by-algorithm]] — Grafici risultati
- [[results/index]] — Risultati storici