#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace mosqueeze {

enum class RawCompression : uint8_t {
    Uncompressed = 0,
    LosslessCompressed = 1,
    LossyCompressed = 2,
    Unknown = 255
};

struct RawFormatInfo {
    std::string_view extension;
    std::string_view manufacturer;
    RawCompression compression = RawCompression::Unknown;
    std::string_view compressionName;
    bool isUncompressedAvailable = false;
    std::string_view notes;
};

class RawFormatDetector {
public:
    static std::optional<RawFormatInfo> detect(std::span<const uint8_t> data);
    static std::optional<RawFormatInfo> detectByExtension(std::string_view filename);
    static bool shouldApplyBayerPreprocessor(RawCompression compression) {
        return compression == RawCompression::Uncompressed;
    }
};

} // namespace mosqueeze
