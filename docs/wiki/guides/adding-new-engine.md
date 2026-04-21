# Adding a New Compression Engine

**Summary**: Procedura pratica per aggiungere un nuovo engine a MoSqueeze.

**Last updated**: 2026-04-22

---

## Required Steps

1. Aggiungere header e source engine sotto `src/libmosqueeze/include/mosqueeze/engines/` e `src/libmosqueeze/src/engines/`.
2. Implementare `ICompressionEngine` completo (`name`, `levels`, `compress`, `decompress`).
3. Aggiornare `src/libmosqueeze/CMakeLists.txt` con nuovo file e librerie richieste.
4. Registrare engine dove serve (`mosqueeze analyze`, benchmark harness, selector rules).
5. Aggiungere unit test roundtrip in `tests/unit/`.
6. Aggiornare wiki (pagina algoritmo + mapping/decision pages).

---

## Example: ZPAQ (Implemented)

### Files Added

- `src/libmosqueeze/include/mosqueeze/engines/ZpaqEngine.hpp`
- `src/libmosqueeze/src/engines/ZpaqEngine.cpp`
- `tests/unit/ZpaqEngine_test.cpp`

### Dependency Strategy

ZPAQ non e in vcpkg nel progetto corrente, quindi usiamo vendoring:

- `third_party/zpaq/libzpaq/libzpaq.h`
- `third_party/zpaq/libzpaq/libzpaq.cpp`
- `third_party/zpaq/CMakeLists.txt`

Root CMake include:

```cmake
add_subdirectory(third_party/zpaq)
```

Lib link:

```cmake
target_link_libraries(libmosqueeze PRIVATE libzpaq)
```

### Streaming Pattern

Anche con librerie storicamente file-based, mantenere contratto streaming:

- adapter `libzpaq::Reader` su `std::istream`
- adapter `libzpaq::Writer` su `std::ostream`
- conversione errori in `std::runtime_error`

### Selector Integration

Aggiornare `AlgorithmSelector` per includere il nuovo engine dove utile (es. `Binary_Database -> zpaq:5`, fallback `lzma:9`).

---

## Testing Checklist

- Roundtrip minimo e correttezza output
- Metadata engine (`name`, extension, levels)
- Regressione suite completa (`ctest`)
- Sanity check CLI (es. `mosqueeze analyze`)

---

## Notes

- Preferire sempre API streaming e buffer chunked.
- Non richiedere installazioni esterne se possibile: vendoring o dipendenze CMake riproducibili.
- Documentare sempre trade-off ratio/speed nella wiki dell'algoritmo.
