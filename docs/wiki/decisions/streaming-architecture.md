# ADR: Streaming Architecture

**Summary**: Perché MoSqueeze usa streaming 64KB вместо di caricare file interi in memoria.

**Status**: Accepted

**Last updated**: 2026-04-22

---

## Context

MoSqueeze è progettato per cold storage di file potenzialmente molto grandi:
- Database dump multi-GB
- Log file di server (mesi di log)
- Backup completi (TB di dati)

## Decision

**Usare API streaming con buffer 64KB per tutte le operazioni di compressione/decompressione.**

```cpp
constexpr size_t BUFFER_SIZE = 64 * 1024;

while (input) {
    input.read(buffer, BUFFER_SIZE);
    compress_chunk(buffer, output);
}
```

---

## Rationale

### 1. Memory Efficiency

| Approccio | File 1GB | File 100GB | File 1TB |
|-----------|----------|------------|----------|
| Load whole | 1GB RAM | 100GB RAM | ❌ OOM |
| Streaming 64KB | 64KB RAM | 64KB RAM | 64KB RAM |

**Conclusione**: Streaming scala linearmente e non fallisce su file grandi.

### 2. Latenza Percepita

Con streaming, l'utente vede progress immediato:
- Load whole: Aspetta X secondi, poi output
- Streaming: Output dopo primi 64KB processati

### 3. Error Recovery

Se processo fallisce a metà:
- Load whole: Output perso completamente
- Streaming: Output parziale fino al punto di fallimento

### 4. Platform Independence

Alcuni ambienti hanno memory constraints:
- Container Docker (limiti memory)
- WSL (meno efficiente)
- Embedded systems (RAM limitata)

---

## Alternatives Considered

### 1. Memory-Mapped Files (mmap)

```cpp
void* mapped = mmap(file, size);
compress(mapped, size);
```

**Pro**: Semplice
**Contro**: 
- Richiede address space contiguo
- Problemi su 32-bit con file >2GB
- Non cross-platform (Windows differente)
- Swapping può causare thrashing

### 2. Buffered I/O con dimamiche dimensioni

```cpp
vector<char> buffer(detect_optimal_chunk_size());
```

**Pro**: Adattivo
**Contro**: 
- Complessità aggiunta
- Heuristics spesso sbagliate
- 64KB è sweet spot per cache lines

### 3. Whole-File con External Sorting

**Pro**: Massima compression ratio (possibilità di dizionario globale)
**Contro**: 
- Temp files
- Disco I/O extra
- Non pratico per cold storage

---

## Implementation Pattern

### ZstdEngine Example

```cpp
CompressionResult ZstdEngine::compress(istream& input, ostream& output, ...) {
    // 1. Inizializza encoder
    ZSTD_CStream* cstream = ZSTD_createCStream();
    RAII guard{ZSTD_freeCStream, cstream};
    
    // 2. Buffer fissi 64KB
    std::array<char, 64 * 1024> inBuffer;
    std::array<char, 64 * 1024> outBuffer;
    
    // 3. Loop streaming
    while (true) {
        input.read(inBuffer.data(), inBuffer.size());
        
        ZSTD_inBuffer in{inBuffer.data(), input.gcount(), 0};
        
        while (in.pos < in.size) {
            ZSTD_outBuffer out{outBuffer.data(), outBuffer.size(), 0};
            ZSTD_compressStream2(cstream, &out, &in, ZSTD_e_continue);
            output.write(outBuffer.data(), out.pos);
        }
        
        if (input.eof()) break;
    }
    
    // 4. Finalize
    // ...
}
```

### LzmaEngine Example

```cpp
void LzmaEngine::compress(istream& input, ostream& output, ...) {
    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_easy_encoder(&strm, level, LZMA_CHECK_CRC64);
    
    uint8_t inBuf[64 * 1024];
    uint8_t outBuf[64 * 1024];
    
    lzma_action action = LZMA_RUN;
    
    while (input) {
        strm.next_in = inBuf;
        strm.avail_in = input.readsome((char*)inBuf, sizeof(inBuf));
        
        if (input.eof()) action = LZMA_FINISH;
        
        do {
            strm.next_out = outBuf;
            strm.avail_out = sizeof(outBuf);
            lzma_code(&strm, action);
            output.write((char*)outBuf, sizeof(outBuf) - strm.avail_out);
        } while (strm.avail_out == 0);
    }
}
```

---

## Consequences

### Positive
- ✅ Memory costante indipendente da file size
- ✅ Cross-platform senza modificationi
- ✅ Progress visibile
- ✅ Scalabile a TB/PB

### Negative
- ⚠️ Overhead function calls per chunk
- ⚠️ Leggermente più lento per file piccoli (<1MB)

### Neutral
- 64KB è tunable ma works well per la maggior parte di casi
- Possibile aggiungere tuning in futuro senza cambiare architettura

---

## Related Pages

- [[zstd]] — Implementazione streaming
- [[lzma-xz]] — Implementazione streaming
- [[brotli]] — Implementazione streaming