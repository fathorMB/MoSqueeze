#pragma once

#include <mosqueeze/ICompressionEngine.hpp>

#include <vector>

namespace mosqueeze {

class ZstdEngine : public ICompressionEngine {
public:
    std::string name() const override { return "zstd"; }
    std::string fileExtension() const override { return ".zst"; }
    std::vector<int> supportedLevels() const override;
    int defaultLevel() const override { return 22; }
    int maxLevel() const override { return 22; }

    CompressionResult compress(
        std::istream& input,
        std::ostream& output,
        const CompressionOptions& opts = {}) override;

    void decompress(
        std::istream& input,
        std::ostream& output) override;
};

} // namespace mosqueeze
