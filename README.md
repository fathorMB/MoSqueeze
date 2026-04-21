# MoSqueeze

MoSqueeze is a cross-platform C++20 cold-storage compression library.

## Requirements

- CMake 3.20+
- C++20 compiler
  - Windows: MSVC 2022
  - Linux: GCC 11+
  - macOS: Clang 14+
- vcpkg (with `VCPKG_ROOT` set)

## Dependencies

Dependencies are declared in `vcpkg.json`:

- zstd
- lzma
- brotli
- cli11
- fmt
- spdlog
- nlohmann-json

## Build

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

## Run CLI

```bash
./build/src/mosqueeze-cli/mosqueeze --version
```

Expected output:

```text
mosqueeze v0.1.0
```

## Run Tests

```bash
cd build
ctest --output-on-failure
```

## Documentation

**📚 [Wiki/Knowledge Base](docs/wiki/)** — Comprehensive documentation with algorithm comparisons, benchmarks, and guides.

### Quick Links

- [Algorithm Comparison Matrix](docs/wiki/algorithms/comparison-matrix.md) — Which algorithm for which file type
- [FileType → Algorithm Mapping](docs/wiki/decisions/file-type-to-algorithm.md) — Decision guide
- [Benchmark Results](docs/wiki/benchmarks/graphs/ratio-by-algorithm.md) — Performance data
- [Getting Started Guide](docs/wiki/guides/getting-started.md) — Full setup instructions

## License

MIT
