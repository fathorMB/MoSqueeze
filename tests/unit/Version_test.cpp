#include <mosqueeze/Version.hpp>

#include <cassert>
#include <string>

int main() {
    const std::string version = mosqueeze::versionString();
    assert(version == "0.1.0");
    return 0;
}
