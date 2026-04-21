#pragma once

#include <mosqueeze/ICompressionEngine.hpp>

#include <memory>
#include <vector>

namespace mosqueeze {

class ZpaqEngine : public ICompressionEngine {
public:
    ZpaqEngine();
    ~ZpaqEngine() override;

    std::string name() const override { return "zpaq"; }
    std::string fileExtension() const override { return ".zpaq"; }
    std::vector<int> supportedLevels() const override;
    int defaultLevel() const override { return 5; }
    int maxLevel() const override { return 5; }

    CompressionResult compress(
        std::istream& input,
        std::ostream& output,
        const CompressionOptions& opts = {}) override;

    void decompress(
        std::istream& input,
        std::ostream& output) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mosqueeze
