# Getting Started

**Summary**: Guida per buildare, testare e contribuire a MoSqueeze.

**Last updated**: 2026-04-22

---

## Prerequisites

### Required

- **C++20 compiler**: GCC 12+, Clang 15+, MSVC 2022+
- **CMake 3.20+**
- **vcpkg** package manager
- **Git**

### Platform-Specific

#### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install build-essential cmake git pkg-config
```

#### macOS
```bash
brew install cmake git
xcode-select --install
```

#### Windows
- Visual Studio 2022 with C++ workload
- vcpkg (via installer or `winget install vcpkg`)

---

## Quick Start

### 1. Clone & Setup vcpkg

```bash
git clone https://github.com/fathorMB/MoSqueeze.git
cd MoSqueeze

# Set vcpkg triplet (optional, defaults to x64-linux/x64-osx/x64-windows)
export VCPKG_DEFAULT_TRIPLET=x64-linux
```

### 2. Configure & Build

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

### 3. Run Tests

```bash
cd build
ctest --output-on-failure
```

### 4. Install (Optional)

```bash
cmake --install build --prefix /usr/local
```

---

## Project Structure

```
MoSqueeze/
├── CMakeLists.txt              # Root CMake
├── vcpkg.json                   # Dependencies
├── src/
│   ├── libmosqueeze/           # Core library
│   │   ├── include/mosqueeze/
│   │   │   ├── ICompressionEngine.hpp
│   │   │   ├── Types.hpp
│   │   │   └── engines/
│   │   └── src/
│   │       ├── engines/
│   │       └── platform/
│   ├── mosqueeze-cli/          # CLI tool
│   │   └── src/main.cpp
│   └── mosqueeze-bench/        # Benchmark harness
│       ├── include/mosqueeze/bench/
│       └── src/
├── tests/
│   └── unit/                   # Unit tests
└── docs/
    ├── features/               # Worker specs
    └── wiki/                   # Knowledge base
```

---

## Development Workflow

### Building Debug

```bash
cmake -B build-debug -S . -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=...
cmake --build build-debug
```

### Running Specific Tests

```bash
# All tests
ctest

# Specific test executable
./build/tests/unit/ZstdEngine_test

# With verbose output
ctest -V
```

### Code Style

- **ClangFormat** configured in `.clang-format`
- **clang-tidy** checks in CI
- Naming: `CamelCase` for types, `snake_case` for functions (follow existing)

### Pre-commit

```bash
# Format check
clang-format --dry-run --Werror src/**/*.cpp

# Static analysis
clang-tidy src/**/*.cpp -- -I src/libmosqueeze/include
```

---

## Dependencies

From `vcpkg.json`:

| Package | Version | Purpose |
|---------|---------|---------|
| `zstd` | latest | Zstandard compression |
| `liblzma` | latest | LZMA/XZ compression |
| `brotli` | latest | Brotli compression |
| `cli11` | latest | CLI parsing |
| `fmt` | latest | Formatting |
| `spdlog` | latest | Logging |
| `nlohmann-json` | latest | JSON handling |
| `sqlite3` | latest | Benchmark results storage |

---

## Running the CLI

### Basic Usage

```bash
# Analyze recommendation
./build/src/mosqueeze-cli/mosqueeze analyze ./data/sample.json

# Intelligent recommendation
./build/src/mosqueeze-cli/mosqueeze suggest ./data/sample.json --goal min-size --json

# Compress with explicit algorithm/level/preprocessor
./build/src/mosqueeze-cli/mosqueeze compress ./data/input.json -o ./data/input.msz -a zstd -l 3 --preprocess json-canonical

# Decompress (default output removes .msz suffix)
./build/src/mosqueeze-cli/mosqueeze decompress ./data/input.msz
```

### Options

```
Usage: mosqueeze [OPTIONS] COMMAND [ARGS...]

Commands:
  analyze       Analyze file and recommend algorithm
  suggest       Intelligent compression suggestion
  compress      Compress a file
  decompress    Decompress a file

Compress options:
  -a, --algorithm    zstd|brotli|lzma|zpaq
  -l, --level        Compression level (algorithm-specific)
      --preprocess   none|json-canonical|xml-canonical|image-meta-strip|png-optimizer|bayer-raw
      --json         Machine-readable result output

Decompress options:
  -o, --output       Output file (default: input without `.msz`)
      --json         Machine-readable result output

Global options:
  --version          Show version
  --help             Show help
```

---

## Running Benchmarks

```bash
# Dry run configuration
./mosqueeze-bench --directory ./corpus --iterations 3 --warmup 1 --dry-run

# Run benchmark on directory with selected engines
./mosqueeze-bench --directory ./corpus --algorithms zstd,lzma,brotli,zpaq --summary

# Export Markdown report
./mosqueeze-bench --directory ./corpus --export ./benchmarks/results/report.md --format markdown
```

For full CLI reference and comparison workflow, see [[benchmark-tool]].

---

## Troubleshooting

### vcpkg not found

```bash
# Set environment variable
export VCPKG_ROOT=/path/to/vcpkg

# Or in CMake
cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake ...
```

### Missing dependencies

```bash
# Manual vcpkg install
vcpkg install zstd liblzma brotli cli11 fmt spdlog nlohmann-json sqlite3
```

### Test failures

```bash
# Verbose test output
ctest -V --output-on-failure

# Run single test with debug
gdb ./build/tests/unit/ZstdEngine_test
```

### Build errors on Windows

- Ensure MSVC 2022 with C++20 support
- Check vcpkg triplet matches (`x64-windows`)
- Use Developer Command Prompt

---

## Contributing

### Workflow

1. Create feature branch from `main`
2. Make changes with tests
3. Ensure CI passes
4. Create PR with description

### PR Checklist

- [ ] Code compiles without warnings (`-Wall -Wextra`)
- [ ] Tests pass (including new tests for changes)
- [ ] Documentation updated if needed
- [ ] Commit messages descriptive

---

## Related Pages

- [[adding-new-engine]] — Adding a new compression algorithm
- [[cold-storage-patterns]] — Best practices for archival
