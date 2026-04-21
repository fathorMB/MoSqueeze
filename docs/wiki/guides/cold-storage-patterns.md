# Cold Storage Patterns

**Summary**: Best practices per archiviazione a lungo termine con compressione massima.

**Last updated**: 2026-04-22

---

## What is Cold Storage?

**Cold storage** = archiviazione di dati raramente accessi ma che devono essere preservati.

### Characteristics

- **Scritto una volta, letto raramente**
- **Tempo di compressione irrilevante** — anche ore va bene
- **Ratio è cruciale** — ogni GB risparmiato conta
- **Integrità è critica** — checksum sempre attivo

### Use Cases

- Backup di database storici
- Log di anni precedenti
- Repository archiviati
- Dataset di machine learning
- Snapshots di sistemi

---

## Compression Strategy

### Algorithm Selection

```
Cold Storage Decision Tree:

1. File già compresso? 
   → SKIP (JPEG, MP4, MP3)
   
2. Testo (JSON/XML/HTML)?
   → brotli quality 11
   
3. Codice sorgente?
   → zstd level 22 ultra
   
4. Binari/Database?
   → lzma level 9 extreme
   
5. Contenuto archivio?
   → EXTRACT + ricomprimi contenuti individualmente
```

### Level Selection

Per cold storage, usare **SEMPRE** il massimo:

| Algorithm | Cold Storage Level | Notes |
|-----------|-------------------|-------|
| zstd | 22 + ultra | `--ultra` flag |
| lzma | 9 extreme | `LZMA_PRESET_EXTREME` |
| brotli | quality 11 | Already at max |

---

## Integrity

### Checksums

Ogni archivio dovrebbe includere checksum:

```
backup.tar.zsinfo
├── backup.tar.zst      (compressed archive)
└── backup.tar.zst.sha256
```

### Format Support

- **XZ (LZMA)** — CRC64 integrato
- **zstd** — Checksum opzionale, abilitare
- **brotli** — No checksum, richiede wrapper

### Verification

```bash
# Post-compression verify
mosqueeze-cli compress data.db data.db.zst --verify

# Periodic integrity check
sha256sum -c data.db.zst.sha256
```

---

## Directory Patterns

### Pattern 1: Type-Organized

```
cold-storage/
├── 2025-Q1/
│   ├── databases/
│   │   ├── users.2025-03-01.db.xz
│   │   └── logs.2025-03-01.db.xz
│   ├── logs/
│   │   ├── access.2025-03.log.zst
│   │   └── app.2025-03.log.zst
│   └── code/
│       └── repo-snapshot.tar.zst
├── 2025-Q2/
│   └── ...
└── index.json         # Manifest of all files
```

### Pattern 2: Date-Partitioned

```
cold-storage/
├── 2025/
│   ├── 01/
│   │   └── backup-2025-01-31.tar.zst
│   ├── 02/
│   └── ...
└── latest -> 2025/04/   # Symlink to latest
```

### Pattern 3: Content-Hash

```
cold-storage/
├── ab/
│   └── cd/
│       └── abcd1234...ef.gz  # Hash-based naming
└── manifest.json           # Maps original → hash
```

---

## Size Estimation

### Before/After Prediction

Basato su FileType:

| File Type | Est. Ratio | Est. Time (per GB) |
|-----------|------------|-------------------|
| Text/JSON | 10-15x | 30-60 seconds |
| Source code | 8-12x | 20-40 seconds |
| Database | 2-3x | 2-5 minutes |
| Binary exe | 2-3x | 3-8 minutes |
| Logs | 5-10x | 1-3 minutes |

### Disk Planning

```
Original size: 100 GB mixed files
Est. compressed: ~15-25 GB
Compression time: ~1-2 hours
Memory required: ~2-4 GB peak
```

---

## Retrieval Strategy

### Partial Retrieval

Per archivi grandi:

```bash
# Don't decompress entire archive
# Use zstd --decompress --output-dir-flat if supported

# For tar.zst:
zstd -d archive.tar.zst | tar -xf - specific/file.txt
```

### Indexing

Mantenere indice esterno:

```json
{
  "archive": "backup-2025-03.tar.zst",
  "files": [
    {"path": "data/users.db", "offset": 0, "size": 1048576},
    {"path": "logs/app.log", "offset": 1048576, "size": 524288}
  ]
}
```

---

## Automation

### Cron Script

```bash
#!/bin/bash
# monthly-cold-storage.sh

ARCHIVE_DIR="/archive/cold-storage"
SOURCE_DIR="/data/to-archive"
DATE=$(date +%Y-%m-%d)

# Create archive directory
mkdir -p "$ARCHIVE_DIR/$DATE"

# Compress with appropriate algorithms
find "$SOURCE_DIR" -name "*.json" -exec \
  mosqueeze-cli compress {} "$ARCHIVE_DIR/$DATE/{}.br" --algorithm brotli --level 11 \;

find "$SOURCE_DIR" -name "*.db" -exec \
  mosqueeze-cli compress {} "$ARCHIVE_DIR/$DATE/{}.xz" --algorithm lzma --level 9 --extreme \;

find "$SOURCE_DIR" -name "*.log" -exec \
  mosqueeze-cli compress {} "$ARCHIVE_DIR/$DATE/{}.zst" --algorithm zstd --level 22 --extreme \;

# Generate manifest
mosqueeze-cli manifest "$ARCHIVE_DIR/$DATE" > "$ARCHIVE_DIR/$DATE/manifest.json"

# Verify all
mosqueeze-cli verify "$ARCHIVE_DIR/$DATE"
```

---

## Monitoring

### Metrics to Track

- Total size before/after
- Compression ratio per file type
- Time taken
- Error rate
- Disk usage trend

### Alerts

- Ratio anomalo (<2x per testo → possibile problema)
- Tempo eccessivo (>10x expected → indagare)
- Checksum mismatch → corruzione

---

## Retention

### Retention Policy

| Data Age | Action |
|----------|--------|
| 0-1 year | Hot storage (uncompressed) |
| 1-3 years | Warm storage (zstd level 19) |
| 3+ years | Cold storage (max compression) |
| 10+ years | Deep archive (zpaq if available) |

### Migration

```bash
# Re-compress from warm to cold
mosqueeze-cli migrate warm-archive/ cold-archive/ --upgrade-level
```

---

## Related Pages

- [[../decisions/file-type-to-algorithm]] — Which algorithm for which file
- [[../decisions/compression-levels]] — Level selection
- [[../benchmarks/methodology]] — Measuring results