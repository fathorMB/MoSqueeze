#pragma once

#include <mosqueeze/ICompressionEngine.hpp>

#include <vector>

namespace mosqueeze {

class LzmaEngine : public ICompressionEngine {
public:
    std::string name() const override { return "xz"; }
    std::string fileExtension() const override { return ".xz"; }
    std::vector<int> supportedLevels() const override;
    int defaultLevel() const override { return 9; }
    int maxLevel() const override { return 9; }

    CompressionResult compress(
        std::istream& input,
        std::ostream& output,
        const CompressionOptions& opts = {}) override;

    void decompress(
        std::istream& input,
        std::ostream& output) override;
};

} // namespace mosqueeze
