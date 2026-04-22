#include "RafParser.hpp"

#include <algorithm>

namespace mosqueeze {
namespace {

constexpr uint16_t kTypeByte = 1;
constexpr uint16_t kTypeAscii = 2;
constexpr uint16_t kTypeShort = 3;
constexpr uint16_t kTypeLong = 4;
constexpr uint16_t kTypeRational = 5;

constexpr uint16_t kTagImageWidth = 256;
constexpr uint16_t kTagImageLength = 257;
constexpr uint16_t kTagBitsPerSample = 258;
constexpr uint16_t kTagStripOffsets = 273;
constexpr uint16_t kTagStripByteCounts = 279;

} // namespace

bool RafParser::isRafFile(const std::vector<uint8_t>& data) {
    static constexpr char kMagic[] = "FUJIFILM";
    if (data.size() < sizeof(kMagic) - 1) {
        return false;
    }
    return std::equal(kMagic, kMagic + (sizeof(kMagic) - 1), data.begin());
}

RafMetadata RafParser::parse(const std::vector<uint8_t>& data) {
    RafMetadata meta{};
    if (!isRafFile(data)) {
        return meta;
    }

    if (data.size() >= 20) {
        meta.cameraId.assign(reinterpret_cast<const char*>(data.data() + 12), 8);
    }

    if (data.size() < 0x38) {
        return meta;
    }

    const uint32_t rafDirectoryOffset =
        static_cast<uint32_t>(data[0x34]) |
        (static_cast<uint32_t>(data[0x35]) << 8U) |
        (static_cast<uint32_t>(data[0x36]) << 16U) |
        (static_cast<uint32_t>(data[0x37]) << 24U);

    size_t tiffBase = 0;
    bool littleEndian = true;
    if (!findTiffBase(data, tiffBase, littleEndian)) {
        return meta;
    }

    // The offset at +4 of the TIFF header is the first IFD offset relative to TIFF base.
    if (tiffBase + 8 > data.size()) {
        return meta;
    }
    const uint32_t firstIfd = readU32(data, tiffBase + 4, littleEndian);

    if (!parseIfd(data, tiffBase, firstIfd, littleEndian, meta)) {
        // Some RAF variants expose a RAF directory offset; try it as absolute fallback.
        if (rafDirectoryOffset > 0 && rafDirectoryOffset < data.size()) {
            (void)parseIfd(data, 0, rafDirectoryOffset, true, meta);
        }
    }

    if (meta.rawImageOffset == 0 || meta.rawImageOffset >= data.size()) {
        return meta;
    }
    if (meta.rawImageSize == 0 && meta.width > 0 && meta.height > 0 && meta.bitsPerSample > 0) {
        const uint64_t estimated =
            (static_cast<uint64_t>(meta.width) * static_cast<uint64_t>(meta.height) * static_cast<uint64_t>(meta.bitsPerSample)) / 8U;
        if (estimated > 0 && estimated <= static_cast<uint64_t>(data.size())) {
            meta.rawImageSize = static_cast<uint32_t>(estimated);
        }
    }
    if (meta.rawImageSize == 0) {
        return meta;
    }
    if (static_cast<uint64_t>(meta.rawImageOffset) + static_cast<uint64_t>(meta.rawImageSize) > data.size()) {
        return meta;
    }

    meta.valid = true;
    return meta;
}

bool RafParser::findTiffBase(const std::vector<uint8_t>& data, size_t& tiffBase, bool& littleEndian) {
    if (data.size() < 8) {
        return false;
    }

    for (size_t i = 0; i + 4 <= data.size() && i < 4096; ++i) {
        const bool isLe = (data[i] == 0x49 && data[i + 1] == 0x49 && data[i + 2] == 0x2A && data[i + 3] == 0x00);
        const bool isBe = (data[i] == 0x4D && data[i + 1] == 0x4D && data[i + 2] == 0x00 && data[i + 3] == 0x2A);
        if (isLe || isBe) {
            tiffBase = i;
            littleEndian = isLe;
            return true;
        }
    }

    return false;
}

bool RafParser::parseIfd(
    const std::vector<uint8_t>& data,
    size_t tiffBase,
    size_t ifdOffset,
    bool littleEndian,
    RafMetadata& meta) {
    const size_t ifdBase = tiffBase + ifdOffset;
    if (ifdBase + 2 > data.size()) {
        return false;
    }

    const uint16_t entryCount = readU16(data, ifdBase, littleEndian);
    size_t entryOffset = ifdBase + 2;
    bool touched = false;

    for (uint16_t i = 0; i < entryCount; ++i) {
        IfdEntry entry{};
        if (!readEntry(data, entryOffset, littleEndian, entry)) {
            break;
        }

        uint32_t value = 0;
        if (parseEntryValueU32(data, tiffBase, littleEndian, entry, value)) {
            switch (entry.tag) {
                case kTagImageWidth:
                    meta.width = value;
                    touched = true;
                    break;
                case kTagImageLength:
                    meta.height = value;
                    touched = true;
                    break;
                case kTagBitsPerSample:
                    meta.bitsPerSample = value;
                    touched = true;
                    break;
                case kTagStripOffsets:
                    meta.rawImageOffset = value;
                    touched = true;
                    break;
                case kTagStripByteCounts:
                    meta.rawImageSize = value;
                    touched = true;
                    break;
                default:
                    break;
            }
        }

        entryOffset += 12;
    }

    return touched;
}

bool RafParser::parseEntryValueU32(
    const std::vector<uint8_t>& data,
    size_t tiffBase,
    bool littleEndian,
    const IfdEntry& entry,
    uint32_t& value) {
    const size_t elemSize = typeSize(entry.type);
    if (elemSize == 0 || entry.count == 0) {
        return false;
    }
    const uint64_t totalBytes = static_cast<uint64_t>(elemSize) * static_cast<uint64_t>(entry.count);
    size_t valueOffset = 0;

    if (totalBytes <= 4) {
        // Value is packed in valueOrOffset.
        if (entry.type == kTypeShort) {
            value = littleEndian ? (entry.valueOrOffset & 0xFFFFU) : ((entry.valueOrOffset >> 16U) & 0xFFFFU);
            return true;
        }
        value = entry.valueOrOffset;
        return true;
    }

    valueOffset = tiffBase + static_cast<size_t>(entry.valueOrOffset);
    if (valueOffset >= data.size()) {
        return false;
    }

    if (entry.type == kTypeShort) {
        if (valueOffset + 2 > data.size()) {
            return false;
        }
        value = readU16(data, valueOffset, littleEndian);
        return true;
    }
    if (entry.type == kTypeLong) {
        if (valueOffset + 4 > data.size()) {
            return false;
        }
        value = readU32(data, valueOffset, littleEndian);
        return true;
    }

    return false;
}

uint16_t RafParser::readU16(const std::vector<uint8_t>& data, size_t offset, bool littleEndian) {
    if (offset + 2 > data.size()) {
        return 0;
    }
    if (littleEndian) {
        return static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8U);
    }
    return static_cast<uint16_t>(data[offset + 1]) | (static_cast<uint16_t>(data[offset]) << 8U);
}

uint32_t RafParser::readU32(const std::vector<uint8_t>& data, size_t offset, bool littleEndian) {
    if (offset + 4 > data.size()) {
        return 0;
    }
    if (littleEndian) {
        return static_cast<uint32_t>(data[offset]) |
            (static_cast<uint32_t>(data[offset + 1]) << 8U) |
            (static_cast<uint32_t>(data[offset + 2]) << 16U) |
            (static_cast<uint32_t>(data[offset + 3]) << 24U);
    }
    return static_cast<uint32_t>(data[offset + 3]) |
        (static_cast<uint32_t>(data[offset + 2]) << 8U) |
        (static_cast<uint32_t>(data[offset + 1]) << 16U) |
        (static_cast<uint32_t>(data[offset]) << 24U);
}

bool RafParser::readEntry(
    const std::vector<uint8_t>& data,
    size_t offset,
    bool littleEndian,
    IfdEntry& entry) {
    if (offset + 12 > data.size()) {
        return false;
    }
    entry.tag = readU16(data, offset, littleEndian);
    entry.type = readU16(data, offset + 2, littleEndian);
    entry.count = readU32(data, offset + 4, littleEndian);
    entry.valueOrOffset = readU32(data, offset + 8, littleEndian);
    return true;
}

size_t RafParser::typeSize(uint16_t type) {
    switch (type) {
        case kTypeByte:
        case kTypeAscii:
            return 1;
        case kTypeShort:
            return 2;
        case kTypeLong:
            return 4;
        case kTypeRational:
            return 8;
        default:
            return 0;
    }
}

} // namespace mosqueeze
