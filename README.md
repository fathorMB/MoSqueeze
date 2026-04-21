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
