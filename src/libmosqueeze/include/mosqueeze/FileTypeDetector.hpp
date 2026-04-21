#pragma once

#include <mosqueeze/Types.hpp>
#include <string>

namespace mosqueeze {

class FileTypeDetector {
public:
    static FileType detectFromPath(const std::string& path);
};

} // namespace mosqueeze
