# Compression Level Selection

**Summary**: Guida alla scelta del livello di compressione per uso vs cold storage.

**Last updated**: 2026-04-22

---

## Overview

Ogni algoritmo ha una scala di livelli. Questa guida aiuta a scegliere quello giusto.

---

## General Rules

### Cold Storage (MoSqueeze Primary Use Case)

```
 Usa SEMPRE il massimo livello:
- zstd: level 22 (--ultra)
- lzma: level 9 extreme
- brotli: quality 11
```

**Perché?**
- Encode time irrilevante per cold storage
- Decode speed non penalizzato
- Massimo space saving
- Una volta solo, leggere molte volte

### Interactive / Real-time

```
Usa livelli bassi:
- zstd: level 3-5
- lzma: level 1-3
- brotli: quality 4-6
```

**Perché?**
- Risposta veloce importante
- Compress-time latenza visibile
- Ratio accettabile per preview

---

## Algorithm-Specific

### zstd Levels

| Level | Ratio | Encode | Decode | Use Case |
|-------|-------|--------|--------|----------|
| **1-3** | 2-2.5x | ⚡⚡⚡⚡⚡ | ⚡⚡⚡⚡⚡ | Real-time streaming |
| **5-7** | 2.5-3x | ⚡⚡⚡⚡ | ⚡⚡⚡⚡⚡ | File transfer veloce |
| **10-15** | 3-3.5x | ⚡⚡⚡ | ⚡⚡⚡⚡⚡ | Backup giornaliero |
| **19** | 3.8x | ⚡⚡ | ⚡⚡⚡⚡ | **Cold storage default** |
| **22 ultra** | 4.2x | ⚡ | ⚡⚡⚡⚡ | **Cold storage max** |

### lzma Levels

| Level | Ratio | Encode | Decode | Use Case |
|-------|-------|--------|--------|----------|
| **1-3** | 2.5-3x | ⚡⚡⚡ | ⚡⚡⚡⚡ | Anteprima rapida |
| **5-6** | 3-3.5x | ⚡⚡ | ⚡⚡⚡⚡ | Backup standard |
| **7-8** | 3.5-4x | ⚡ | ⚡⚡⚡ | Cold storage good |
| **9 extreme** | 4-4.5x | ⚡ (slow) | ⚡⚡⚡ | **Cold storage max** |

### brotli Quality

| Quality | Ratio | Encode | Decode | Use Case |
|---------|-------|--------|--------|----------|
| **4-6** | 2.5-3x | ⚡⚡⚡⚡ | ⚡⚡⚡⚡⚡ | Web serving |
| **8-9** | 3.5-4x | ⚡⚡ | ⚡⚡⚡⚡ | Static assets |
| **11** | 4-4.5x | ⚡ (slow) | ⚡⚡⚡⚡⚡ | **Cold storage** |

---

## Trade-off Analysis

### Ratio vs Encode Time

```
Ratio improvement: 10% (level 19 → 22)
Encode time: 4x slower
Decode time: same
Memory: 2x more
```

**Per cold storage**: Worthy trade-off
**Per interactive**: Not worth it

### Memory vs Level

zstd memory at various levels:

| Level | Memory |
|-------|--------|
| 3 | 32 MB |
| 10 | 64 MB |
| 19 | 128 MB |
| 22 | 256 MB |

LZMA:

| Level | Dictionary | Memory |
|-------|------------|--------|
| 1 | 64KB | 32 MB |
| 5 | 1MB | 64 MB |
| 9 | 64MB | 512 MB |

---

## MoSqueeze Defaults

### CompressionOptions

```cpp
struct CompressionOptions {
    int level = 22;       // zstd default
    bool extreme = true;  // LZMA preset extreme / zstd ultra
    bool verify = false;  // Decompress after to verify
};
```

### FileType-Specific Defaults

```cpp
int defaultLevel(FileType type) {
    switch (type) {
        case Text_*: return 19; // zstd or 11 for brotli
        case Binary_*: return 9; // lzma
        case Image_PNG: return 19; // zstd
        default: return 19;
    }
}
```

---

## Decision Flowchart

```mermaid
flowchart TD
    A[Start] --> B{Cold Storage?}
    
    B -->|Yes| C{File Type?}
    B -->|No| D{Real-time?}
    
    C -->|Text| E[brotli-11 or zstd-22]
    C -->|Binary| F[lzma-9 extreme]
    C -->|Mixed Archive| G[zstd-22 ultra]
    
    D -->|Yes| H[zstd-3]
    D -->|No (batch)| I{Time budget?}
    
    I -->|<1s/file| H
    I -->|Minutes| J[zstd-19]
    I -->|Hours| K[zstd-22 or max algo]
```

---

## Benchmarks Impact

Esempio reale su corpus 100MB:

| Config | Ratio | Time | Result Size |
|--------|-------|------|-------------|
| zstd-3 | 2.8x | 0.3s | 35.7 MB |
| zstd-19 | 4.1x | 1.2s | 24.4 MB |
| zstd-22 | 4.3x | 4.8s | 23.3 MB |

**Risparmio level 19 vs 22**: 1.1 MB
**Tempo extra**: 3.6s

Per cold storage di 1TB:
- Level 19: 244 GB
- Level 22: 233 GB  
- **Risparmio**: 11 GB con 4 ore extra CPU

---

## Related Pages

- [[comparison-matrix]] — Confronto algoritmi
- [[file-type-to-algorithm]] — Mappatura by FileType
- [[streaming-architecture]] — ADR su streaming