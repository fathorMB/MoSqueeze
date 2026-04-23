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
- sqlite3
- pugixml
- zlib

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

## Run Benchmark Tool

```bash
# List engines and levels
./build/src/mosqueeze-bench/mosqueeze-bench --list-engines

# Benchmark a directory with multiple iterations
./build/src/mosqueeze-bench/mosqueeze-bench --directory ./benchmarks/corpus --iterations 5 --warmup 2 --summary

# Parallel run with explicit thread count + JSON/CSV exports
./build/src/mosqueeze-bench/mosqueeze-bench --directory ./benchmarks/corpus --threads 8 --output ./benchmarks/results --json --csv

# Run benchmark with automatic preprocessing selection
./build/src/mosqueeze-bench/mosqueeze-bench --directory ./benchmarks/corpus --preprocess auto --default-only --verbose

# Check RAW format detection/compression classification for Bayer preprocessing
./build/src/mosqueeze-bench/mosqueeze-bench --directory ./raw --preprocess bayer-raw --dry-run

# Force Bayer preprocessing even when RAW is detected as compressed
./build/src/mosqueeze-bench/mosqueeze-bench --directory ./raw --preprocess bayer-raw --force-bayer

# PNG optimization with explicit engine selection
./build/src/mosqueeze-bench/mosqueeze-bench --directory ./benchmarks/corpus --preprocess png-optimizer --png-engine oxipng --png-level 3

# Export HTML report
./build/src/mosqueeze-bench/mosqueeze-bench --directory ./benchmarks/corpus --export ./benchmarks/results/report.html --format html
```

## RAW format support

Bayer preprocessing now classifies RAW payloads before transforming:

- `Uncompressed`: processed normally
- `Lossless compressed`: skipped by default (no expected gain), unless `--force-bayer`
- `Lossy compressed`: rejected to avoid harmful transforms

Current detector coverage includes: RAF, ARW, CR2, CR3, NEF/NRW (by extension), IIQ, 3FR, DNG.

## Run Tests

```bash
cd build
ctest --output-on-failure

# Run only preprocessor-focused tests
ctest -R "BayerPreprocessor_test|ImageMetaStripper_test|JsonCanonicalizer_test|XmlCanonicalizer_test|DictionaryPreprocessor_test|PreprocessorSelector_test" --output-on-failure
```

## Documentation

**📚 [Wiki/Knowledge Base](docs/wiki/)** — Comprehensive documentation with algorithm comparisons, benchmarks, and guides.

### Quick Links

- [Algorithm Comparison Matrix](docs/wiki/algorithms/comparison-matrix.md) — Which algorithm for which file type
- [FileType → Algorithm Mapping](docs/wiki/decisions/file-type-to-algorithm.md) — Decision guide
- [Benchmark Results](docs/wiki/benchmarks/graphs/ratio-by-algorithm.md) — Performance data
- [Benchmark Tool Guide](docs/wiki/guides/benchmark-tool.md) — Full `mosqueeze-bench` CLI and exports
- [Benchmark Visualization Guide](docs/wiki/guides/benchmark-visualization.md) — `mosqueeze-viz` dashboards and comparisons
- [Preprocessing Guide](docs/wiki/preprocessing.md) — Current preprocessors and trailing header format
- [Getting Started Guide](docs/wiki/guides/getting-started.md) — Full setup instructions

## License

MIT
