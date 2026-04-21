#pragma once

#include <mosqueeze/ICompressionEngine.hpp>

#include <vector>

namespace mosqueeze {

class BrotliEngine : public ICompressionEngine {
public:
    std::string name() const override { return "brotli"; }
    std::string fileExtension() const override { return ".br"; }
    std::vector<int> supportedLevels() const override;
    int defaultLevel() const override { return 11; }
    int maxLevel() const override { return 11; }

    CompressionResult compress(
        std::istream& input,
        std::ostream& output,
        const CompressionOptions& opts = {}) override;

    void decompress(
        std::istream& input,
        std::ostream& output) override;
};

} // namespace mosqueeze
