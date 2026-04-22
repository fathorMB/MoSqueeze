#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mosqueeze {

struct RafMetadata {
    std::string cameraId;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t bitsPerSample = 0;
    uint32_t rawImageOffset = 0;
    uint32_t rawImageSize = 0;
    bool valid = false;
};

class RafParser {
public:
    static bool isRafFile(const std::vector<uint8_t>& data);
    static RafMetadata parse(const std::vector<uint8_t>& data);

private:
    struct IfdEntry {
        uint16_t tag = 0;
        uint16_t type = 0;
        uint32_t count = 0;
        uint32_t valueOrOffset = 0;
    };

    static bool findTiffBase(const std::vector<uint8_t>& data, size_t& tiffBase, bool& littleEndian);
    static bool parseIfd(
        const std::vector<uint8_t>& data,
        size_t tiffBase,
        size_t ifdOffset,
        bool littleEndian,
        RafMetadata& meta);
    static bool parseEntryValueU32(
        const std::vector<uint8_t>& data,
        size_t tiffBase,
        bool littleEndian,
        const IfdEntry& entry,
        uint32_t& value);
    static uint16_t readU16(const std::vector<uint8_t>& data, size_t offset, bool littleEndian);
    static uint32_t readU32(const std::vector<uint8_t>& data, size_t offset, bool littleEndian);
    static bool readEntry(
        const std::vector<uint8_t>& data,
        size_t offset,
        bool littleEndian,
        IfdEntry& entry);
    static size_t typeSize(uint16_t type);
};

} // namespace mosqueeze
