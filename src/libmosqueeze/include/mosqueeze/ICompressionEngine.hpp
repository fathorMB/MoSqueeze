#pragma once

#include <istream>
#include <mosqueeze/Types.hpp>
#include <ostream>
#include <string>
#include <vector>

namespace mosqueeze {

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
