#include "preprocessors/RafParser.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

void writeLe16(std::vector<uint8_t>& data, size_t offset, uint16_t value) {
    data[offset] = static_cast<uint8_t>(value & 0xFFU);
    data[offset + 1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

void writeLe32(std::vector<uint8_t>& data, size_t offset, uint32_t value) {
    data[offset] = static_cast<uint8_t>(value & 0xFFU);
    data[offset + 1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    data[offset + 2] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    data[offset + 3] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

std::vector<uint8_t> makeSyntheticRaf() {
    constexpr size_t kSize = 0x200;
    constexpr size_t kTiffBase = 0x80;
    constexpr uint32_t kFirstIfd = 0x08;
    constexpr size_t kIfd = kTiffBase + kFirstIfd;
    constexpr size_t kRawOffset = 0x140;
    constexpr uint32_t kRawSize = 32;

    std::vector<uint8_t> data(kSize, 0x00);
    const char* magic = "FUJIFILM";
    for (size_t i = 0; i < 8; ++i) {
        data[i] = static_cast<uint8_t>(magic[i]);
    }

    writeLe32(data, 0x34, static_cast<uint32_t>(kIfd));
    data[kTiffBase] = 0x49;
    data[kTiffBase + 1] = 0x49;
    data[kTiffBase + 2] = 0x2A;
    data[kTiffBase + 3] = 0x00;
    writeLe32(data, kTiffBase + 4, kFirstIfd);

    writeLe16(data, kIfd, 5);
    size_t entry = kIfd + 2;

    // width
    writeLe16(data, entry, 256);
    writeLe16(data, entry + 2, 4);
    writeLe32(data, entry + 4, 1);
    writeLe32(data, entry + 8, 4);
    entry += 12;

    // height
    writeLe16(data, entry, 257);
    writeLe16(data, entry + 2, 4);
    writeLe32(data, entry + 4, 1);
    writeLe32(data, entry + 8, 4);
    entry += 12;

    // bits per sample
    writeLe16(data, entry, 258);
    writeLe16(data, entry + 2, 3);
    writeLe32(data, entry + 4, 1);
    writeLe32(data, entry + 8, 16);
    entry += 12;

    // strip offsets
    writeLe16(data, entry, 273);
    writeLe16(data, entry + 2, 4);
    writeLe32(data, entry + 4, 1);
    writeLe32(data, entry + 8, static_cast<uint32_t>(kRawOffset));
    entry += 12;

    // strip byte counts
    writeLe16(data, entry, 279);
    writeLe16(data, entry + 2, 4);
    writeLe32(data, entry + 4, 1);
    writeLe32(data, entry + 8, kRawSize);

    for (size_t i = 0; i < kRawSize; ++i) {
        data[kRawOffset + i] = static_cast<uint8_t>(i);
    }

    return data;
}

} // namespace

int main() {
    {
        std::vector<uint8_t> valid = {'F', 'U', 'J', 'I', 'F', 'I', 'L', 'M'};
        assert(mosqueeze::RafParser::isRafFile(valid));
    }
    {
        std::vector<uint8_t> invalid = {'I', 'I', '*', 0x00};
        assert(!mosqueeze::RafParser::isRafFile(invalid));
    }

    const auto raf = makeSyntheticRaf();
    const auto meta = mosqueeze::RafParser::parse(raf);
    assert(meta.valid);
    assert(meta.width == 4);
    assert(meta.height == 4);
    assert(meta.bitsPerSample == 16);
    assert(meta.rawImageOffset == 0x140);
    assert(meta.rawImageSize == 32);

    std::cout << "[PASS] RafParser_test\n";
    return 0;
}
