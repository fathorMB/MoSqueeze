# Video Compression for Cold Storage

## Overview

MoSqueeze supporta compressione video per cold storage usando ZPAQ level 5 su AVI e MKV.

## File Types Supportati

| Estensione | Tipo | Strategia | Ratio tipico |
|------------|------|-----------|--------------|
| `.avi` | Video_AVI | ZPAQ 5 | 1.1-10x |
| `.mkv` | Video_MKV | ZPAQ 5 | 1.05-1.15x |
| `.mp4` | Video_MP4 | Skip | - |
| `.webm` | Video_WebM | Skip | - |
| `.mov` | Video_MP4 | Skip | - |

## Perche ZPAQ per video?

I video sono gia compressi con codec come H.264, H.265, VP9. Tuttavia:

1. Container overhead: MKV/AVI hanno struttura che puo essere compressa.
2. Context mixing: ZPAQ trova correlazioni residue tra blocchi.
3. Metadata: tracce e metadata comprimono bene.
4. Cold storage: encode time irrilevante, ogni bit conta.

## Ratio Attesi

### AVI Raw/DV
- Input: video non compresso
- ZPAQ level 5: 3-10x ratio
- Confronto: ZSTD level 22 tipicamente inferiore

### AVI DivX/XviD
- Input: gia compresso
- ZPAQ level 5: 1.1-1.3x ratio

### MKV H.264/H.265
- Input: stream gia compressi
- ZPAQ level 5: 1.05-1.15x ratio

## Quando NON comprimere

- Hot storage con accesso frequente
- Streaming dove il decode time e critico
- MP4/WebM: beneficio tipicamente troppo basso rispetto al costo CPU

## Note implementative in MoSqueeze

- `Video_AVI` e un `FileType` dedicato.
- AVI e rilevato sia da estensione (`.avi`) sia da magic bytes RIFF/AVI.
- `Video_MKV` e trattato come recompressabile per strategia cold-storage.
- `AlgorithmSelector` usa:
  - `Video_AVI -> zpaq level 5` (fallback `lzma level 9`)
  - `Video_MKV -> zpaq level 5` (fallback `zstd level 22`)
