#include <mosqueeze/CompressionPipeline.hpp>

#include <mosqueeze/preprocessors/DictionaryPreprocessor.hpp>
#include <mosqueeze/preprocessors/BayerPreprocessor.hpp>
#include <mosqueeze/preprocessors/ImageMetaStripper.hpp>
#include <mosqueeze/preprocessors/JsonCanonicalizer.hpp>
#include <mosqueeze/preprocessors/PngOptimizer.hpp>
#include <mosqueeze/preprocessors/XmlCanonicalizer.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace mosqueeze {
namespace {

uint32_t readU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
        (static_cast<uint32_t>(p[1]) << 8U) |
        (static_cast<uint32_t>(p[2]) << 16U) |
        (static_cast<uint32_t>(p[3]) << 24U);
}

void writeU32LE(std::ostream& out, uint32_t value) {
    uint8_t bytes[4] = {
        static_cast<uint8_t>(value & 0xFFU),
        static_cast<uint8_t>((value >> 8U) & 0xFFU),
        static_cast<uint8_t>((value >> 16U) & 0xFFU),
        static_cast<uint8_t>((value >> 24U) & 0xFFU)
    };
    out.write(reinterpret_cast<const char*>(bytes), 4);
}

std::unique_ptr<IPreprocessor> createPreprocessor(PreprocessorType type) {
    switch (type) {
        case PreprocessorType::JsonCanonicalizer:
            return std::make_unique<JsonCanonicalizer>();
        case PreprocessorType::XmlCanonicalizer:
            return std::make_unique<XmlCanonicalizer>();
        case PreprocessorType::ImageMetaStripper:
            return std::make_unique<ImageMetaStripper>();
        case PreprocessorType::DictionaryPreprocessor:
            return std::make_unique<DictionaryPreprocessor>();
        case PreprocessorType::BayerPreprocessor:
            return std::make_unique<BayerPreprocessor>();
        case PreprocessorType::PngOptimizer:
            return std::make_unique<PngOptimizer>();
        case PreprocessorType::None:
        default:
            return nullptr;
    }
}

FileType detectFileType(const std::string& raw) {
    if (raw.size() >= 3 &&
        static_cast<uint8_t>(raw[0]) == 0xFFU &&
        static_cast<uint8_t>(raw[1]) == 0xD8U &&
        static_cast<uint8_t>(raw[2]) == 0xFFU) {
        return FileType::Image_JPEG;
    }

    if (raw.size() >= 8 &&
        static_cast<uint8_t>(raw[0]) == 0x89U &&
        raw[1] == 'P' && raw[2] == 'N' && raw[3] == 'G' &&
        static_cast<uint8_t>(raw[4]) == 0x0DU &&
        static_cast<uint8_t>(raw[5]) == 0x0AU &&
        static_cast<uint8_t>(raw[6]) == 0x1AU &&
        static_cast<uint8_t>(raw[7]) == 0x0AU) {
        return FileType::Image_PNG;
    }

    if (raw.size() >= 4) {
        const uint8_t b0 = static_cast<uint8_t>(raw[0]);
        const uint8_t b1 = static_cast<uint8_t>(raw[1]);
        const uint8_t b2 = static_cast<uint8_t>(raw[2]);
        const uint8_t b3 = static_cast<uint8_t>(raw[3]);
        if ((b0 == 0x49U && b1 == 0x49U && b2 == 0x2AU && b3 == 0x00U) ||
            (b0 == 0x4DU && b1 == 0x4DU && b2 == 0x00U && b3 == 0x2AU)) {
            return FileType::Image_Raw;
        }
    }

    size_t i = 0;
    while (i < raw.size() && std::isspace(static_cast<unsigned char>(raw[i])) != 0) {
        ++i;
    }
    if (i < raw.size() && (raw[i] == '{' || raw[i] == '[' || raw[i] == '<')) {
        return FileType::Text_Structured;
    }

    return FileType::Unknown;
}

} // namespace

CompressionPipeline::CompressionPipeline(ICompressionEngine* engine)
    : engine_(engine) {
    if (engine_ == nullptr) {
        throw std::runtime_error("CompressionPipeline requires a non-null engine");
    }
}

void CompressionPipeline::setPreprocessor(IPreprocessor* preprocessor) {
    preprocessor_ = preprocessor;
}

PipelineResult CompressionPipeline::compress(
    std::istream& input,
    std::ostream& output,
    const CompressionOptions& opts) {
    const std::string raw((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    PipelineResult pipelineResult{};
    std::string payload = raw;

    if (preprocessor_ != nullptr) {
        const FileType fileType = detectFileType(raw);
        if (preprocessor_->canProcess(fileType)) {
            std::istringstream preInput(raw);
            std::ostringstream preOutput;
            pipelineResult.preprocessing = preprocessor_->preprocess(preInput, preOutput, fileType);
            payload = preOutput.str();
            pipelineResult.wasPreprocessed = true;
        } else {
            pipelineResult.preprocessing.type = PreprocessorType::None;
            pipelineResult.preprocessing.originalBytes = raw.size();
            pipelineResult.preprocessing.processedBytes = raw.size();
        }
    } else {
        pipelineResult.preprocessing.type = PreprocessorType::None;
        pipelineResult.preprocessing.originalBytes = raw.size();
        pipelineResult.preprocessing.processedBytes = raw.size();
    }

    std::istringstream engineIn(payload);
    std::ostringstream compressedOut;
    pipelineResult.compression = engine_->compress(engineIn, compressedOut, opts);

    const std::string compressedPayload = compressedOut.str();
    output.write(compressedPayload.data(), static_cast<std::streamsize>(compressedPayload.size()));
    writeTrailingHeader(output, pipelineResult.preprocessing);
    return pipelineResult;
}

void CompressionPipeline::decompress(
    std::istream& input,
    std::ostream& output) {
    const std::vector<uint8_t> payload((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    size_t compressedEnd = payload.size();
    PreprocessResult meta{};
    (void)readTrailingHeader(payload, compressedEnd, meta);

    const std::string compressed(payload.begin(), payload.begin() + static_cast<std::ptrdiff_t>(compressedEnd));
    std::istringstream compressedIn(compressed);
    std::ostringstream decompressedOut;
    engine_->decompress(compressedIn, decompressedOut);

    if (meta.type == PreprocessorType::None) {
        output << decompressedOut.str();
        return;
    }

    std::unique_ptr<IPreprocessor> ownedPreprocessor;
    IPreprocessor* processor = preprocessor_;
    if (processor == nullptr || processor->type() != meta.type) {
        ownedPreprocessor = createPreprocessor(meta.type);
        processor = ownedPreprocessor.get();
    }

    if (processor == nullptr) {
        output << decompressedOut.str();
        return;
    }

    std::istringstream postIn(decompressedOut.str());
    processor->postprocess(postIn, output, meta);
}

void CompressionPipeline::writeTrailingHeader(std::ostream& output, const PreprocessResult& meta) const {
    writeU32LE(output, MAGIC);
    const uint8_t typeByte = static_cast<uint8_t>(meta.type);
    output.write(reinterpret_cast<const char*>(&typeByte), 1);

    if (meta.metadata.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
        throw std::runtime_error("Preprocessor metadata too large");
    }
    writeU32LE(output, static_cast<uint32_t>(meta.metadata.size()));
    if (!meta.metadata.empty()) {
        output.write(
            reinterpret_cast<const char*>(meta.metadata.data()),
            static_cast<std::streamsize>(meta.metadata.size()));
    }
}

bool CompressionPipeline::readTrailingHeader(
    const std::vector<uint8_t>& payload,
    size_t& compressedEndOffset,
    PreprocessResult& result) const {
    if (payload.size() < 9) {
        result = {};
        compressedEndOffset = payload.size();
        return false;
    }

    const size_t minStart = payload.size() > (1024 * 1024) ? payload.size() - (1024 * 1024) : 0;
    for (size_t i = minStart; i + 9 <= payload.size(); ++i) {
        const uint32_t magic = readU32LE(payload.data() + i);
        if (magic != MAGIC) {
            continue;
        }

        const auto type = static_cast<PreprocessorType>(payload[i + 4]);
        const uint32_t metadataLen = readU32LE(payload.data() + i + 5);
        const size_t end = i + 9 + static_cast<size_t>(metadataLen);
        if (end != payload.size()) {
            continue;
        }

        result = {};
        result.type = type;
        result.metadata.assign(payload.begin() + static_cast<std::ptrdiff_t>(i + 9), payload.end());
        compressedEndOffset = i;
        return true;
    }

    result = {};
    compressedEndOffset = payload.size();
    return false;
}

} // namespace mosqueeze
