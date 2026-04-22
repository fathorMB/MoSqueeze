#pragma once

#include <mosqueeze/ICompressionEngine.hpp>
#include <mosqueeze/Preprocessor.hpp>

#include <cstdint>
#include <istream>
#include <ostream>
#include <vector>

namespace mosqueeze {

struct PipelineResult {
    CompressionResult compression;
    PreprocessResult preprocessing;
    bool wasPreprocessed = false;
};

class CompressionPipeline {
public:
    explicit CompressionPipeline(ICompressionEngine* engine);

    void setPreprocessor(IPreprocessor* preprocessor);

    PipelineResult compress(
        std::istream& input,
        std::ostream& output,
        const CompressionOptions& opts = {});

    void decompress(
        std::istream& input,
        std::ostream& output);

private:
    ICompressionEngine* engine_;
    IPreprocessor* preprocessor_ = nullptr;

    static constexpr uint32_t MAGIC = 0x4D535146U;

    void writeTrailingHeader(std::ostream& output, const PreprocessResult& meta) const;
    bool readTrailingHeader(
        const std::vector<uint8_t>& payload,
        size_t& compressedEndOffset,
        PreprocessResult& result) const;
};

} // namespace mosqueeze
