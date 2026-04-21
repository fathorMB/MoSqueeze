# FileType → Algorithm Mapping

**Summary**: Mappatura completa tra tipi di file e algoritmo raccomandato.

**Last updated**: 2026-04-22

---

## Decision Matrix

### Text Files

| FileType | Algoritmo | Livello | Motivazione |
|----------|-----------|---------|-------------|
| Text_SourceCode | zstd | 19 | Alta compressione, decode veloce per IDE |
| Text_SourceCode (web) | brotli | 11 | JS/CSS ottimizzati da brotli |
| Text_Prose / Markdown | brotli | 11 | Testo naturale, brotli eccelle |
| Text_Structured (JSON) | brotli | 11 | Miglior ratio per JSON |
| Text_Structured (XML) | brotli | 11 | Simile a JSON |
| Text_Structured (YAML/TOML) | zstd | 19 | Config files |
| Text_Log (structured) | lzma | 9 | Pattern ripetitivi timestamp |
| Text_Log (raw) | zstd | 19 | Bilanciamento speed/ratio |

### Image Files

| FileType | Algoritmo | Livello | Motivazione |
|----------|-----------|---------|-------------|
| Image_PNG | zstd | 19 | PNG compresso ma migliorabile |
| Image_JPEG | **SKIP** | — | Già compresso, ricomprimere danneggia |
| Image_GIF | zstd | 19 | LZW già applicato, ma meglio zstd |
| Image_WebP | **SKIP** | — | Già compresso |
| Image_Raw (BMP/TIFF) | lzma | 9 | Pixel data non compresso |

### Video Files

| FileType | Algoritmo | Livello | Motivazione |
|----------|-----------|---------|-------------|
| Video_MP4 | **SKIP** | — | H.264/265 già compresso |
| Video_MKV | **SKIP** | — | Container con stream compressi |
| Video_WebM | **SKIP** | — | VP9/AV1 compresso |

### Audio Files

| FileType | Algoritmo | Livello | Motivazione |
|----------|-----------|---------|-------------|
| Audio_WAV | lzma | 9 | PCM non compresso |
| Audio_MP3 | **SKIP** | — | Già compresso con lossy |
| Audio_FLAC | **SKIP** | — | Lossless già compresso |

### Binary Files

| FileType | Algoritmo | Livello | Motivazione |
|----------|-----------|---------|-------------|
| Binary_Executable (ELF/EXE) | lzma | 9 | Pattern ripetitivi in binari |
| Binary_Database (SQLite) | lzma | 9 | Struttura tabellare comprime bene |
| Binary_Chunked | zstd | 19 | Dipende dal contenuto |

### Archives

| FileType | Algoritmo | Azione | Motivazione |
|----------|-----------|--------|-------------|
| Archive_ZIP | **EXTRACT + RECOMPRESS** | — | Contenuti spesso meglio con zstd/lzma |
| Archive_TAR | zstd | 19 | TAR non compresso |
| Archive_7Z | **EXTRACT + RECOMPRESS** | — | LZMA già usato ma può migliorare |

---

## Regole Generali

### 1. Skip Rule

```
if (file.isCompressed && !file.canRecompress) {
    SKIP; // JPEG, MP4, MP3, WebP, etc.
}
```

**Perché?** Questi formati usano compressione lossy dedicata. Ricomprimerli:
- Peggiora quality (artefatti aggiuntivi)
- Spesso aumenta size (overhead compressione)
- Spreca CPU

### 2. Re-compress Rule

```
if (file.isCompressed && file.canRecompress) {
    zstd level 19; // PNG, GIF, FLAC
}
```

**Perché?** Formati lossless possono beneficiare di algoritmi moderni.

### 3. Extract-then-Compress Rule

```
if (file.type == Archive_ZIP || file.type == Archive_7Z) {
    extract_contents();
    compress_each_with_best_algorithm();
}
```

**Perché?** Contenuti individuali comprimono meglio dell'archivio intero.

---

## Esempi Pratici

### Scenario 1: Backup Directory Mista

```
backup/
├── src/           → zstd-19 (codice)
├── assets/
│   ├── logo.png   → zstd-19 (PNG migliora 5-10%)
│   └── photo.jpg  → SKIP (già compresso)
├── data/
│   ├── users.db   → lzma-9 (SQLite)
│   └── config.json → brotli-11
└── logs/
    └── app.log    → lzma-9 (pattern)
```

### Scenario 2: Progetto React

```
react-app/
├── src/           → brotli-11 (JS/JSX)
├── public/
│   └── index.html → brotli-11
├── dist/
│   └── bundle.js  → brotli-11 (production)
└── package.json   → brotli-11
```

### Scenario 3: Database Dump

```
database/
├── dump.sql       → zstd-19 (SQL text)
├── snapshots/
│   └── data.db    → lzma-9 (binary)
└── migrations/
    └── 001.sql    → zstd-19
```

---

## Related Pages

- [[zstd]] — Algoritmo details
- [[lzma-xz]] — Algoritmo details
- [[brotli]] — Algoritmo details
- [[comparison-matrix]] — Tabella completa
- [[guides/cold-storage-patterns]] — Best practices