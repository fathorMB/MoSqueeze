#include <mosqueeze/RawFormat.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace mosqueeze {
namespace {

bool hasPrefix(std::span<const uint8_t> data, std::span<const uint8_t> prefix) {
    return data.size() >= prefix.size() &&
        std::equal(prefix.begin(), prefix.end(), data.begin());
}

bool hasTiffHeader(std::span<const uint8_t> data) {
    if (data.size() < 4) {
        return false;
    }
    const bool le = (data[0] == 0x49 && data[1] == 0x49 && data[2] == 0x2A && data[3] == 0x00);
    const bool be = (data[0] == 0x4D && data[1] == 0x4D && data[2] == 0x00 && data[3] == 0x2A);
    return le || be;
}

std::string toUpper(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return out;
}

RawFormatInfo makeFormat(
    std::string_view extension,
    std::string_view manufacturer,
    RawCompression compression,
    std::string_view compressionName,
    bool isUncompressedAvailable,
    std::string_view notes) {
    return RawFormatInfo{
        extension,
        manufacturer,
        compression,
        compressionName,
        isUncompressedAvailable,
        notes
    };
}

} // namespace

std::optional<RawFormatInfo> RawFormatDetector::detect(std::span<const uint8_t> data) {
    static constexpr std::array<uint8_t, 8> kRafMagic = {'F', 'U', 'J', 'I', 'F', 'I', 'L', 'M'};
    static constexpr std::array<uint8_t, 4> kIiqMagic = {'I', 'I', 'Q', 0x00};
    static constexpr std::array<uint8_t, 3> k3frMagicA = {'3', 'F', 'R'};
    static constexpr std::array<uint8_t, 3> k3frMagicB = {'H', '3', 'D'};
    static constexpr std::array<uint8_t, 8> kSonyMagic = {'S', 'O', 'N', 'Y', ' ', 'D', 'S', 'C'};

    if (hasPrefix(data, kRafMagic)) {
        RawCompression compression = RawCompression::Unknown;
        std::string_view compressionName = "Unknown";
        if (data.size() > 84) {
            const uint8_t marker = data[84];
            if (marker == 0) {
                compression = RawCompression::Uncompressed;
                compressionName = "Uncompressed RAW";
            } else if (marker == 1) {
                compression = RawCompression::LosslessCompressed;
                compressionName = "Lossless Compressed RAW";
            } else if (marker == 2) {
                compression = RawCompression::LossyCompressed;
                compressionName = "Lossy Compressed RAW";
            }
        }
        return makeFormat(
            ".RAF",
            "Fujifilm",
            compression,
            compressionName,
            true,
            "Fujifilm default is often lossless compressed");
    }

    if (hasPrefix(data, kIiqMagic)) {
        return makeFormat(
            ".IIQ",
            "Phase One",
            RawCompression::Uncompressed,
            "Uncompressed RAW",
            true,
            "Phase One IIQ is typically uncompressed");
    }

    if (hasPrefix(data, k3frMagicA) || hasPrefix(data, k3frMagicB)) {
        return makeFormat(
            ".3FR",
            "Hasselblad",
            RawCompression::Uncompressed,
            "Uncompressed RAW",
            true,
            "Hasselblad 3FR is typically uncompressed");
    }

    if (hasPrefix(data, kSonyMagic)) {
        return makeFormat(
            ".ARW",
            "Sony",
            RawCompression::Unknown,
            "Unknown",
            true,
            "Check camera setting for uncompressed RAW mode");
    }

    if (data.size() >= 12 && hasTiffHeader(data) && data[8] == 'C' && data[9] == 'R') {
        return makeFormat(
            ".CR2",
            "Canon",
            RawCompression::Unknown,
            "Unknown",
            true,
            "CR2 can be compressed or uncompressed depending on camera setting");
    }

    if (data.size() >= 12 &&
        data[4] == 'f' && data[5] == 't' && data[6] == 'y' && data[7] == 'p') {
        return makeFormat(
            ".CR3",
            "Canon",
            RawCompression::Unknown,
            "Unknown",
            true,
            "CR3 tile compression depends on source settings");
    }

    return std::nullopt;
}

std::optional<RawFormatInfo> RawFormatDetector::detectByExtension(std::string_view filename) {
    const size_t dot = filename.find_last_of('.');
    if (dot == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string ext = toUpper(filename.substr(dot));
    if (ext == ".RAF") {
        return makeFormat(
            ".RAF",
            "Fujifilm",
            RawCompression::LosslessCompressed,
            "Lossless Compressed RAW (assumed)",
            true,
            "Most modern RAF files are lossless compressed by default");
    }
    if (ext == ".IIQ") {
        return makeFormat(
            ".IIQ",
            "Phase One",
            RawCompression::Uncompressed,
            "Uncompressed RAW",
            true,
            "IIQ is usually uncompressed");
    }
    if (ext == ".3FR") {
        return makeFormat(
            ".3FR",
            "Hasselblad",
            RawCompression::Uncompressed,
            "Uncompressed RAW",
            true,
            "3FR is usually uncompressed");
    }
    if (ext == ".ARW") {
        return makeFormat(
            ".ARW",
            "Sony",
            RawCompression::Unknown,
            "Unknown",
            true,
            "May be compressed or uncompressed");
    }
    if (ext == ".NEF" || ext == ".NRW") {
        return makeFormat(
            ext == ".NEF" ? ".NEF" : ".NRW",
            "Nikon",
            RawCompression::Unknown,
            "Unknown",
            true,
            "Nikon often uses lossless compressed RAW by default");
    }
    if (ext == ".CR2" || ext == ".CR3") {
        return makeFormat(
            ext == ".CR2" ? ".CR2" : ".CR3",
            "Canon",
            RawCompression::Unknown,
            "Unknown",
            true,
            "Canon RAW compression depends on camera setting");
    }
    if (ext == ".DNG") {
        return makeFormat(
            ".DNG",
            "Adobe",
            RawCompression::Unknown,
            "Depends on source",
            true,
            "DNG inherits compression characteristics from source");
    }
    return std::nullopt;
}

} // namespace mosqueeze
