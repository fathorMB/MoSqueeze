#include <mosqueeze/preprocessors/BayerPreprocessor.hpp>

#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace mosqueeze {
namespace {

void writeU32LE(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFFU));
    out.push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<uint8_t>((value >> 24U) & 0xFFU));
}

bool readU32LE(const std::vector<uint8_t>& in, size_t offset, uint32_t& value) {
    if (offset + 4 > in.size()) {
        return false;
    }
    value = static_cast<uint32_t>(in[offset]) |
        (static_cast<uint32_t>(in[offset + 1]) << 8U) |
        (static_cast<uint32_t>(in[offset + 2]) << 16U) |
        (static_cast<uint32_t>(in[offset + 3]) << 24U);
    return true;
}

} // namespace

PreprocessResult BayerPreprocessor::preprocess(
    std::istream& input,
    std::ostream& output,
    FileType fileType) {
    if (!canProcess(fileType)) {
        throw std::runtime_error("BayerPreprocessor cannot process this file type");
    }

    const std::string raw((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const size_t originalSize = raw.size();
    const size_t words = originalSize / 2;

    std::vector<uint8_t> transformed;
    transformed.resize(originalSize);

    for (size_t i = 0; i < words; ++i) {
        transformed[i] = static_cast<uint8_t>(raw[(i * 2)]);
        transformed[words + i] = static_cast<uint8_t>(raw[(i * 2) + 1]);
    }
    if ((originalSize % 2) != 0U) {
        transformed[words * 2] = static_cast<uint8_t>(raw.back());
    }

    output.write(reinterpret_cast<const char*>(transformed.data()), static_cast<std::streamsize>(transformed.size()));

    PreprocessResult result{};
    result.type = PreprocessorType::BayerPreprocessor;
    result.originalBytes = originalSize;
    result.processedBytes = transformed.size();
    result.metadata.reserve(5);
    result.metadata.push_back(1); // metadata version
    writeU32LE(result.metadata, static_cast<uint32_t>(originalSize));
    return result;
}

void BayerPreprocessor::postprocess(
    std::istream& input,
    std::ostream& output,
    const PreprocessResult& result) {
    std::vector<uint8_t> processed((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    if (result.metadata.size() < 5 || result.metadata[0] != 1) {
        output.write(reinterpret_cast<const char*>(processed.data()), static_cast<std::streamsize>(processed.size()));
        return;
    }

    uint32_t originalSize32 = 0;
    if (!readU32LE(result.metadata, 1, originalSize32)) {
        output.write(reinterpret_cast<const char*>(processed.data()), static_cast<std::streamsize>(processed.size()));
        return;
    }
    const size_t originalSize = static_cast<size_t>(originalSize32);
    if (processed.size() != originalSize) {
        output.write(reinterpret_cast<const char*>(processed.data()), static_cast<std::streamsize>(processed.size()));
        return;
    }

    const size_t words = originalSize / 2;
    std::vector<uint8_t> restored;
    restored.resize(originalSize);
    for (size_t i = 0; i < words; ++i) {
        restored[i * 2] = processed[i];
        restored[(i * 2) + 1] = processed[words + i];
    }
    if ((originalSize % 2) != 0U) {
        restored[words * 2] = processed[words * 2];
    }

    output.write(reinterpret_cast<const char*>(restored.data()), static_cast<std::streamsize>(restored.size()));
}

BayerPattern BayerPreprocessor::detectPattern(const std::vector<uint8_t>& data) {
    if (data.size() >= 4) {
        const bool isTiffLe =
            data[0] == 0x49 && data[1] == 0x49 && data[2] == 0x2A && data[3] == 0x00;
        const bool isTiffBe =
            data[0] == 0x4D && data[1] == 0x4D && data[2] == 0x00 && data[3] == 0x2A;
        if (isTiffLe || isTiffBe) {
            return BayerPattern::RGGB;
        }
    }
    return BayerPattern::Unknown;
}

std::optional<RawMetadata> BayerPreprocessor::extractMetadata(const std::vector<uint8_t>& data) {
    RawMetadata meta{};
    meta.pattern = detectPattern(data);
    if (meta.pattern == BayerPattern::Unknown) {
        return std::nullopt;
    }
    return meta;
}

} // namespace mosqueeze
