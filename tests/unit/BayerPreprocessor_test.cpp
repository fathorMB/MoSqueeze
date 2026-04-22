#include <mosqueeze/preprocessors/BayerPreprocessor.hpp>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
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

std::vector<uint8_t> makeSyntheticRaf(size_t& rawOffset, size_t& rawSize) {
    constexpr size_t kSize = 0x240;
    constexpr size_t kTiffBase = 0x80;
    constexpr uint32_t kFirstIfd = 0x08;
    constexpr size_t kIfd = kTiffBase + kFirstIfd;
    rawOffset = 0x140;
    rawSize = 32;

    std::vector<uint8_t> data(kSize, 0xA5);
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
    writeLe16(data, entry, 256);
    writeLe16(data, entry + 2, 4);
    writeLe32(data, entry + 4, 1);
    writeLe32(data, entry + 8, 4);
    entry += 12;

    writeLe16(data, entry, 257);
    writeLe16(data, entry + 2, 4);
    writeLe32(data, entry + 4, 1);
    writeLe32(data, entry + 8, 4);
    entry += 12;

    writeLe16(data, entry, 258);
    writeLe16(data, entry + 2, 3);
    writeLe32(data, entry + 4, 1);
    writeLe32(data, entry + 8, 16);
    entry += 12;

    writeLe16(data, entry, 273);
    writeLe16(data, entry + 2, 4);
    writeLe32(data, entry + 4, 1);
    writeLe32(data, entry + 8, static_cast<uint32_t>(rawOffset));
    entry += 12;

    writeLe16(data, entry, 279);
    writeLe16(data, entry + 2, 4);
    writeLe32(data, entry + 4, 1);
    writeLe32(data, entry + 8, static_cast<uint32_t>(rawSize));

    for (size_t i = 0; i < rawSize; ++i) {
        data[rawOffset + i] = static_cast<uint8_t>(i);
    }
    for (size_t i = rawOffset + rawSize; i < data.size(); ++i) {
        data[i] = 0x5A;
    }

    return data;
}

} // namespace

int main() {
    mosqueeze::BayerPreprocessor preprocessor;
    assert(preprocessor.canProcess(mosqueeze::FileType::Image_Raw));
    assert(!preprocessor.canProcess(mosqueeze::FileType::Image_JPEG));

    // Non-RAF RAW payload must be pass-through (metadata version 0).
    std::vector<uint8_t> nonRaf = {
        0x49, 0x49, 0x2A, 0x00, 0x08, 0x00, 0x00, 0x00
    };
    for (int i = 0; i < 32; ++i) {
        nonRaf.push_back(static_cast<uint8_t>((i * 7) & 0xFF));
    }
    {
        const std::string original(reinterpret_cast<const char*>(nonRaf.data()), nonRaf.size());
        std::istringstream in(original);
        std::ostringstream transformed;
        const auto result = preprocessor.preprocess(in, transformed, mosqueeze::FileType::Image_Raw);
        assert(result.type == mosqueeze::PreprocessorType::BayerPreprocessor);
        assert(result.originalBytes == original.size());
        assert(result.processedBytes == original.size());
        assert(result.metadata.size() == 1);
        assert(result.metadata[0] == 0);
        assert(transformed.str() == original);
    }

    // RAF payload: transform image region only, keep header/trailer intact.
    size_t rawOffset = 0;
    size_t rawSize = 0;
    std::vector<uint8_t> raf = makeSyntheticRaf(rawOffset, rawSize);
    const std::string original(reinterpret_cast<const char*>(raf.data()), raf.size());

    std::istringstream in(original);
    std::ostringstream transformed;
    const auto result = preprocessor.preprocess(in, transformed, mosqueeze::FileType::Image_Raw);
    assert(result.type == mosqueeze::PreprocessorType::BayerPreprocessor);
    assert(result.originalBytes == original.size());
    assert(result.processedBytes == original.size());
    assert(result.metadata.size() == 13);
    assert(result.metadata[0] == 2);
    const std::string transformedText = transformed.str();
    assert(transformedText.size() == original.size());
    assert(transformedText != original);

    for (size_t i = 0; i < rawOffset; ++i) {
        assert(transformedText[i] == original[i]);
    }
    for (size_t i = rawOffset + rawSize; i < original.size(); ++i) {
        assert(transformedText[i] == original[i]);
    }

    std::istringstream transformedIn(transformedText);
    std::ostringstream restored;
    preprocessor.postprocess(transformedIn, restored, result);
    assert(restored.str() == original);

    const auto pattern = mosqueeze::BayerPreprocessor::detectPattern(nonRaf);
    assert(pattern == mosqueeze::BayerPattern::RGGB);
    const auto meta = mosqueeze::BayerPreprocessor::extractMetadata(nonRaf);
    assert(meta.has_value());
    assert(meta->pattern == mosqueeze::BayerPattern::RGGB);

    std::cout << "[PASS] BayerPreprocessor_test\n";
    return 0;
}
