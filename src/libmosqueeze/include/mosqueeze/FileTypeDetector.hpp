#pragma once

#include <mosqueeze/FileClassification.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mosqueeze {

class FileTypeDetector {
public:
    FileTypeDetector();

    FileClassification detect(const std::filesystem::path& path);

    // Detection by magic bytes (most reliable)
    FileClassification detectFromMagic(const std::vector<uint8_t>& buffer) const;

    // Extension-based fallback detection
    FileClassification detectFromExtension(const std::string& ext) const;

    // Text sniffing (UTF-8/plain code/log/structured)
    FileClassification detectTextContent(const std::vector<uint8_t>& buffer) const;

private:
    struct MagicEntry {
        std::vector<uint8_t> bytes;
        size_t offset = 0;
        FileType type = FileType::Unknown;
        std::string mime;
        bool isCompressed = false;
        bool canRecompress = true;
    };

    std::vector<MagicEntry> magicEntries_;

    void registerCommonTypes();
};

} // namespace mosqueeze
