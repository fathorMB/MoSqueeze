#include <mosqueeze/Version.hpp>

#include <string>

namespace mosqueeze {

const char* versionString() {
    static const std::string version =
        std::to_string(VERSION_MAJOR) + "." +
        std::to_string(VERSION_MINOR) + "." +
        std::to_string(VERSION_PATCH);

    return version.c_str();
}

} // namespace mosqueeze
