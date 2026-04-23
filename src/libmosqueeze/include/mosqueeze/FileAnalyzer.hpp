#pragma once

#include <mosqueeze/FileTypeDetector.hpp>
#include <mosqueeze/RawFormat.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace mosqueeze {

struct FileFeatures {
    FileClassification classification{};
    std::string detectedType = "application/octet-stream";
    std::string extension;
    size_t fileSize = 0;
    double entropy = 0.0;
    double chunkRatio = 0.0;
    size_t repeatPatterns = 0;
    bool isStructured = false;
    bool hasMetadata = false;
    std::optional<RawFormatInfo> rawFormat;
};

class FileAnalyzer {
public:
    FileAnalyzer() = default;

    FileFeatures analyze(const std::filesystem::path& file) const;
    FileFeatures analyze(std::span<const uint8_t> data, std::string_view filenameHint = "") const;

private:
    FileTypeDetector detector_{};

    static double computeEntropy(std::span<const uint8_t> data);
    static size_t countRepeatedPatterns(std::span<const uint8_t> data);
    static double estimateChunkRatio(std::span<const uint8_t> data);
    static std::string normalizeExtension(std::string_view filenameHint);
    static bool detectStructured(FileType type, std::string_view mimeType);
};

} // namespace mosqueeze
