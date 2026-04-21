# ZPAQ

**Summary**: Algoritmo ad altissima compressione per cold storage estremo.

## Quando usarlo

- Database storici
- Backup cold storage con scrittura rara
- Dataset dove il ratio massimo conta piu del tempo di encode

## Trade-off

- Ratio: in genere migliore di zstd/lzma su input altamente comprimibili
- Encode: molto lento (ordine minuti per GB, dipende dal contenuto)
- Decode: piu lento di zstd, tipicamente accettabile per restore sporadici

## Configurazione in MoSqueeze

- Engine: `ZpaqEngine`
- Estensione: `.zpaq`
- Livelli supportati: `1..5`
- Default cold storage: `5`

## Note implementative

- `libzpaq` e vendorizzato in `third_party/zpaq/libzpaq/`
- Integrazione via adapter `Reader`/`Writer` su stream C++
- Errori `libzpaq` convertiti in `std::runtime_error`

## Selezione algoritmo

Nel selector attuale, `Binary_Database` preferisce:

1. `zpaq` livello 5 (primary)
2. `lzma` livello 9 (fallback)
