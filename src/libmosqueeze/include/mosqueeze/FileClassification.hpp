#pragma once

#include <mosqueeze/Types.hpp>

#include <string>

namespace mosqueeze {

struct FileClassification {
    FileType type = FileType::Unknown;
    std::string mimeType = "application/octet-stream";
    std::string extension;
    bool isCompressed = false;
    bool canRecompress = true;
    std::string recommendation = "compress";
};

} // namespace mosqueeze
