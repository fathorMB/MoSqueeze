#pragma once

#include <mosqueeze/Types.hpp>
#include <string>

namespace mosqueeze {

class AlgorithmSelector {
public:
    static std::string selectDefaultEngine(FileType fileType);
};

} // namespace mosqueeze
