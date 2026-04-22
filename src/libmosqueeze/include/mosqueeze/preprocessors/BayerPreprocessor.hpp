#pragma once

#include <mosqueeze/Preprocessor.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace mosqueeze {

enum class BayerPattern : uint8_t {
    RGGB = 0,
    GRBG = 1,
    GBRG = 2,
    BGGR = 3,
    Unknown = 255
};

struct RawMetadata {
    uint32_t width = 0;
    uint32_t height = 0;
    uint16_t bitsPerSample = 16;
    uint16_t samplesPerPixel = 1;
    BayerPattern pattern = BayerPattern::Unknown;
    uint16_t blackLevel = 0;
    uint16_t whiteLevel = 0;
    std::array<uint16_t, 4> cfaRepeatPattern{2, 2, 0, 0};
};

class BayerPreprocessor : public IPreprocessor {
public:
    PreprocessorType type() const override { return PreprocessorType::BayerPreprocessor; }
    std::string name() const override { return "bayer-raw"; }
    bool canProcess(FileType fileType) const override { return fileType == FileType::Image_Raw; }

    PreprocessResult preprocess(
        std::istream& input,
        std::ostream& output,
        FileType fileType) override;

    void postprocess(
        std::istream& input,
        std::ostream& output,
        const PreprocessResult& result) override;

    double estimatedImprovement(FileType fileType) const override {
        return canProcess(fileType) ? 0.08 : 0.0;
    }

    static BayerPattern detectPattern(const std::vector<uint8_t>& data);
    static std::optional<RawMetadata> extractMetadata(const std::vector<uint8_t>& data);
};

} // namespace mosqueeze
