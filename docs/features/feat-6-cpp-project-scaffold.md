# Worker Spec: C++ Project Scaffold (Issue #6)

## Overview

Set up the cross-platform C++ project structure for MoSqueeze compression library.

## Context

MoSqueeze is a **cold-storage compression library** that optimizes for maximum compression ratio regardless of encode time. It intelligently selects the best algorithm per file type based on empirical benchmark data.

**Key design principle:** Streaming compression — never load entire files in memory.

## Target Platforms

- Windows (MSVC 2022)
- Linux (GCC 11+)
- macOS (Clang 14+)

## Requirements

### 1. CMake Build System

CMake 3.20+, C++20 standard.

### 2. vcpkg Dependencies

Create `vcpkg.json` with:

```json
{
  "name": "mosqueeze",
  "version": "0.1.0",
  "dependencies": [
    "zstd",
    "lzma",
    "brotli",
    "cli11",
    "fmt",
    "spdlog",
    "nlohmann-json"
  ]
}
```

### 3. Project Structure

Create this directory layout:

```
MoSqueeze/
├── CMakeLists.txt                 # Root CMake
├── vcpkg.json
├── .gitignore                     # Update for C++
├── README.md                      # Update with C++ info
├── cmake/
│   └── vcpkg.cmake                # vcpkg toolchain helpers
├── .vscode/
│   ├── extensions.json            # Recommended extensions
│   ├── settings.json              # Workspace settings
│   ├── launch.json                # Debug configurations
│   ├── tasks.json                 # Build tasks
│   └── c_cpp_properties.json      # IntelliSense config
├── src/
│   ├── libmosqueeze/
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── mosqueeze/
│   │   │       ├── Version.hpp
│   │   │       ├── Types.hpp
│   │   │       ├── ICompressionEngine.hpp
│   │   │       ├── AlgorithmSelector.hpp
│   │   │       └── FileTypeDetector.hpp
│   │   └── src/
│   │       ├── Version.cpp
│   │       └── platform/
│   │           ├── Platform.hpp
│   │           └── Platform.cpp
│   ├── mosqueeze-cli/
│   │   ├── CMakeLists.txt
│   │   └── src/
│   │       └── main.cpp
│   └── mosqueeze-bench/
│       ├── CMakeLists.txt
│       └── src/
│           └── main.cpp
├── tests/
│   ├── CMakeLists.txt
│   └── unit/
│       └── Version_test.cpp
├── benchmarks/
│   └── corpus/
│       └── README.md
└── .github/
    └── workflows/
        └── ci.yml
```

### 4. Core Headers

#### Version.hpp

```cpp
#pragma once

namespace mosqueeze {

// Semantic version components
constexpr int VERSION_MAJOR = 0;
constexpr int VERSION_MINOR = 1;
constexpr int VERSION_PATCH = 0;

const char* versionString();

} // namespace mosqueeze
```

#### Types.hpp

```cpp
#pragma once
#include <chrono>
#include <string>
#include <cstdint>

namespace mosqueeze {

struct CompressionOptions {
    int level = 22;           // Algorithm-specific level
    bool extreme = true;      // Enable ultra-slow modes (cold storage)
    bool verify = false;      // Decompress and verify after
    size_t dictionarySize = 0; // 0 = auto
};

struct CompressionResult {
    size_t originalBytes = 0;
    size_t compressedBytes = 0;
    std::chrono::milliseconds duration{0};
    size_t peakMemoryBytes = 0;
    
    double ratio() const { 
        return compressedBytes > 0 
            ? static_cast<double>(originalBytes) / compressedBytes 
            : 0.0; 
    }
};

// FileType classification enum
enum class FileType {
    // Text
    Text_SourceCode,
    Text_Prose,
    Text_Structured,  // JSON, XML, YAML
    Text_Log,
    
    // Images
    Image_Raw,
    Image_PNG,
    Image_JPEG,
    Image_WebP,
    
    // Video (skip re-compression)
    Video_MP4,
    Video_MKV,
    Video_WebM,
    
    // Audio
    Audio_WAV,
    Audio_MP3,
    Audio_FLAC,
    
    // Binary
    Binary_Executable,
    Binary_Database,
    Binary_Chunked,
    
    // Archives
    Archive_ZIP,
    Archive_TAR,
    Archive_7Z,
    
    Unknown
};

} // namespace mosqueeze
```

#### ICompressionEngine.hpp

```cpp
#pragma once
#include <mosqueeze/Types.hpp>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace mosqueeze {

// Forward declaration
class ICompressionEngine {
public:
    virtual ~ICompressionEngine() = default;
    
    // Metadata
    virtual std::string name() const = 0;
    virtual std::string fileExtension() const = 0;
    virtual std::vector<int> supportedLevels() const = 0;
    virtual int defaultLevel() const = 0;
    virtual int maxLevel() const = 0;
    
    // Operations - streaming
    virtual CompressionResult compress(
        std::istream& input,
        std::ostream& output,
        const CompressionOptions& opts = {}) = 0;
    
    virtual void decompress(
        std::istream& input,
        std::ostream& output) = 0;
};

} // namespace mosqueeze
```

#### Platform.hpp

```cpp
#pragma once

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define MOSQUEEZE_WINDOWS 1
#elif defined(__linux__)
    #define MOSQUEEZE_LINUX 1
#elif defined(__APPLE__)
    #define MOSQUEEZE_MACOS 1
#endif

namespace mosqueeze::platform {

// Get current process memory usage in bytes
size_t getPeakMemoryUsage();

// Get number of logical CPUs
unsigned int getProcessorCount();

} // namespace mosqueeze::platform
```

### 5. CLI Entry Point

mosqueeze-cli should accept basic commands:

```cpp
// main.cpp
#include <mosqueeze/Version.hpp>
#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

int main(int argc, char** argv) {
    CLI::App app{"MoSqueeze - Cold Storage Compression Library"};
    
    // Version flag
    app.add_flag("-V,--version", [](int) {
        fmt::print("mosqueeze v{}\n", mosqueeze::versionString());
        throw CLI::Success();
    }, "Print version and exit");
    
    // For now, subcommands will be added in later issues
    // Just basic parse for scaffold verification
    
    CLI11_PARSE(app, argc, argv);
    
    return 0;
}
```

### 6. GitHub Actions CI

Create `.github/workflows/ci.yml`:

```yaml
name: CI

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]

jobs:
  build-windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v11
        with:
          vcpkgGitCommitId: 'a34c873a9717bb888ab2f1c77b81dab4e8d40d88'
      
      - name: Configure CMake
        run: cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
      
      - name: Build
        run: cmake --build build --config Release
      
      - name: Run tests
        working-directory: build
        run: ctest -C Release --output-on-failure

  build-linux:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake g++-11
          
      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v11
        with:
          vcpkgGitCommitId: 'a34c873a9717bb888ab2f1c77b81dab4e8d40d88'
      
      - name: Configure CMake
        run: cmake -B build -S . -DCMAKE_CXX_COMPILER=g++-11 -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
      
      - name: Build
        run: cmake --build build --config Release
      
      - name: Run tests
        working-directory: build
        run: ctest --output-on-failure

  build-macos:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v11
        with:
          vcpkgGitCommitId: 'a34c873a9717bb888ab2f1c77b81dab4e8d40d88'
      
      - name: Configure CMake
        run: cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
      
      - name: Build
        run: cmake --build build --config Release
      
      - name: Run tests
        working-directory: build
        run: ctest --output-on-failure
```

### 7. VS Code Workspace Files

Create `.vscode/` directory with the following files:

#### `.vscode/extensions.json` (Recommended Extensions)

```json
{
  "recommendations": [
    "ms-vscode.cpptools",
    "ms-vscode.cmake-tools",
    "twxs.cmake",
    "ms-vscode.cpptools-extension-pack"
  ]
}
```

#### `.vscode/settings.json`

```json
{
  "cmake.configureOnOpen": true,
  "cmake.configureSettings": {
    "CMAKE_TOOLCHAIN_FILE": "${env:VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
  },
  "cmake.buildDirectory": "${workspaceFolder}/build",
  "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
  "C_Cpp.errorSquiggles": "Enabled",
  "files.associations": {
    "*.hpp": "cpp",
    "*.cpp": "cpp",
    "CMakeLists.txt": "cmake",
    "*.cmake": "cmake"
  },
  "editor.formatOnSave": true,
  "editor.tabSize": 4,
  "editor.insertSpaces": true,
  "files.trimTrailingWhitespace": true,
  "files.insertFinalNewline": true
}
```

#### `.vscode/launch.json` (Debug Configurations)

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug mosqueeze-cli",
      "type": "cppdbg",
      "request": "launch",
      "program": "${command:cmake.launchTargetPath}",
      "args": ["--version"],
      "cwd": "${workspaceFolder}",
      "environment": [],
      "stopAtEntry": false,
      "externalConsole": false,
      "MIMode": "lldb" 
    },
    {
      "name": "Debug mosqueeze-cli (Windows)",
      "type": "cppdbg",
      "request": "launch",
      "program": "${command:cmake.launchTargetPath}",
      "args": ["--version"],
      "cwd": "${workspaceFolder}",
      "environment": [],
      "stopAtEntry": false,
      "externalConsole": false,
      "MIMode": "windows"
    }
  ]
}
```

#### `.vscode/tasks.json` (Build Tasks)

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "CMake: Configure",
      "type": "shell",
      "command": "cmake",
      "args": [
        "-B", "build",
        "-S", ".",
        "-DCMAKE_TOOLCHAIN_FILE=${env:VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      ],
      "problemMatcher": []
    },
    {
      "label": "CMake: Build (Debug)",
      "type": "shell",
      "command": "cmake",
      "args": ["--build", "build", "--config", "Debug"],
      "group": {
        "kind": "build",
        "isDefault": true
      },
      "problemMatcher": ["$gcc", "$msCompile"]
    },
    {
      "label": "CMake: Build (Release)",
      "type": "shell",
      "command": "cmake",
      "args": ["--build", "build", "--config", "Release"],
      "group": "build",
      "problemMatcher": ["$gcc", "$msCompile"]
    },
    {
      "label": "CMake: Run Tests",
      "type": "shell",
      "command": "ctest",
      "args": ["--test-dir", "build", "--output-on-failure"],
      "group": "test",
      "problemMatcher": []
    }
  ]
}
```

#### `.vscode/c_cpp_properties.json` (IntelliSense)

```json
{
  "configurations": [
    {
      "name": "Windows",
      "includePath": [
        "${workspaceFolder}/src/libmosqueeze/include",
        "${env:VCPKG_ROOT}/installed/x64-windows/include"
      ],
      "compilerPath": "cl.exe",
      "cStandard": "c23",
      "cppStandard": "c++20",
      "intelliSenseMode": "windows-msvc-x64"
    },
    {
      "name": "Linux",
      "includePath": [
        "${workspaceFolder}/src/libmosqueeze/include",
        "${env:VCPKG_ROOT}/installed/x64-linux/include"
      ],
      "compilerPath": "/usr/bin/g++",
      "cStandard": "c23",
      "cppStandard": "c++20",
      "intelliSenseMode": "linux-gcc-x64"
    },
    {
      "name": "macOS",
      "includePath": [
        "${workspaceFolder}/src/libmosqueeze/include",
        "${env:VCPKG_ROOT}/installed/arm64-osx/include"
      ],
      "compilerPath": "/usr/bin/clang++",
      "cStandard": "c23",
      "cppStandard": "c++20",
      "intelliSenseMode": "macos-clang-arm64"
    }
  ],
  "version": 4
}
```

**Note:** After installing VS Code extensions, CMake Tools will auto-detect the configuration and provide:
- Status bar buttons: 🔨 Build, ▶️ Debug, 🔍 Configure
- CMake: Configure runs automatically on project open
- F5 launches the selected target with debugger attached

### 8. .gitignore Updates

Add C++-specific entries:

```gitignore
# Build directories
build/
cmake-build-*/
out/

# IDE
.vscode/
.idea/
*.suo
*.user
*.userprefs
*.sln.docstates

# Compiled
*.o
*.obj
*.exe
*.dll
*.so
*.dylib
*.a
*.lib

# CMake
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
Makefile

# vcpkg
vcpkg_installed/
```

## Acceptance Criteria

1. ✅ CMake builds successfully on Windows, Linux, macOS via GitHub Actions
2. ✅ vcpkg dependencies resolve correctly
3. ✅ `mosqueeze --version` prints `mosqueeze v0.1.0`
4. ✅ `libmosqueeze` shared library builds
5. ✅ Empty stub tests pass
6. ✅ README.md updated with C++ build instructions

## Commands to Verify

```bash
# Configure
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build

# Run CLI
./build/src/mosqueeze-cli/mosqueeze --version

# Run tests
cd build && ctest --output-on-failure
```

## Notes

- Don't implement actual compression logic yet — that's issue #7
- Don't implement file detection yet — that's issue #8
- This scaffold just sets up the build system, headers, and CI

## Definition of Done

- All files created as specified
- CI passes on all 3 platforms
- CLI runs and shows version
- PR created linking to issue #6