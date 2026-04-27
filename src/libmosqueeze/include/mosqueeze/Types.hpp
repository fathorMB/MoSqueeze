#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>

namespace mosqueeze {

struct CompressionOptions {
    int level = 22;            // Algorithm-specific level
    bool extreme = true;       // Enable ultra-slow modes (cold storage)
    bool verify = false;       // Decompress and verify after
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
    Text_Structured, // JSON, XML, YAML
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
    Video_AVI,

    // Audio
    Audio_WAV,
    Audio_MP3,
    Audio_FLAC,

    // Binary
    Binary_Executable,
    Binary_Database,
    Binary_Chunked,

    // Documents
    Document_PDF,

    // Archives
    Archive_ZIP,
    Archive_TAR,
    Archive_7Z,

    Unknown
};

struct SelectionConfig {
    double entropyThreshold = 7.5;
    std::set<std::string> skipExtensions;
    std::set<FileType> skipFileTypes;
};

} // namespace mosqueeze
