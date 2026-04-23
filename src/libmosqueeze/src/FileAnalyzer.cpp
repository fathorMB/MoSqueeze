#include <mosqueeze/FileAnalyzer.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>

namespace mosqueeze {
namespace {

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool startsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

} // namespace

FileFeatures FileAnalyzer::analyze(const std::filesystem::path& file) const {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open file for analysis: " + file.string());
    }

    in.seekg(0, std::ios::end);
    const std::streamoff streamSize = in.tellg();
    in.seekg(0, std::ios::beg);

    constexpr size_t kMaxProbeBytes = 256 * 1024;
    const size_t toRead = streamSize > 0
        ? static_cast<size_t>(std::min<std::streamoff>(streamSize, static_cast<std::streamoff>(kMaxProbeBytes)))
        : 0;

    std::vector<uint8_t> data(toRead);
    if (toRead > 0) {
        in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(toRead));
        data.resize(static_cast<size_t>(in.gcount()));
    }

    FileFeatures features = analyze(data, file.filename().string());
    features.fileSize = streamSize > 0 ? static_cast<size_t>(streamSize) : data.size();
    if (features.extension.empty()) {
        features.extension = toLower(file.extension().string());
    }
    return features;
}

FileFeatures FileAnalyzer::analyze(std::span<const uint8_t> data, std::string_view filenameHint) const {
    std::vector<uint8_t> buffer(data.begin(), data.end());

    FileClassification classification = detector_.detectFromMagic(buffer);
    if (classification.type == FileType::Unknown) {
        const std::string ext = normalizeExtension(filenameHint);
        if (!ext.empty()) {
            classification = detector_.detectFromExtension(ext);
        }
    }
    if (classification.type == FileType::Unknown) {
        classification = detector_.detectTextContent(buffer);
    }

    FileFeatures features{};
    features.classification = classification;
    features.detectedType = classification.mimeType;
    features.extension = normalizeExtension(filenameHint);
    features.fileSize = data.size();
    features.entropy = computeEntropy(data);
    features.repeatPatterns = countRepeatedPatterns(data);
    features.chunkRatio = estimateChunkRatio(data);
    features.isStructured = detectStructured(classification.type, classification.mimeType);

    const auto byContent = RawFormatDetector::detect(data);
    if (byContent.has_value()) {
        features.rawFormat = byContent;
    } else if (!filenameHint.empty()) {
        features.rawFormat = RawFormatDetector::detectByExtension(filenameHint);
    }

    const bool imageLike = startsWith(features.detectedType, "image/");
    const bool textLike = startsWith(features.detectedType, "text/");
    features.hasMetadata = imageLike || features.rawFormat.has_value() || (textLike && features.isStructured);
    return features;
}

double FileAnalyzer::computeEntropy(std::span<const uint8_t> data) {
    if (data.empty()) {
        return 0.0;
    }

    std::array<size_t, 256> counts{};
    for (const auto value : data) {
        ++counts[value];
    }

    const double total = static_cast<double>(data.size());
    double entropy = 0.0;
    for (const size_t count : counts) {
        if (count == 0) {
            continue;
        }
        const double p = static_cast<double>(count) / total;
        entropy -= p * std::log2(p);
    }
    return entropy;
}

size_t FileAnalyzer::countRepeatedPatterns(std::span<const uint8_t> data) {
    if (data.size() < 4) {
        return 0;
    }

    std::unordered_map<uint32_t, size_t> counts;
    const size_t limit = std::min<size_t>(data.size() - 3, 64 * 1024);
    counts.reserve(limit / 4);

    for (size_t i = 0; i < limit; ++i) {
        const uint32_t key =
            (static_cast<uint32_t>(data[i]) << 24U) |
            (static_cast<uint32_t>(data[i + 1]) << 16U) |
            (static_cast<uint32_t>(data[i + 2]) << 8U) |
            static_cast<uint32_t>(data[i + 3]);
        ++counts[key];
    }

    size_t repeats = 0;
    for (const auto& [key, count] : counts) {
        (void)key;
        if (count > 1) {
            repeats += (count - 1);
        }
    }
    return repeats;
}

double FileAnalyzer::estimateChunkRatio(std::span<const uint8_t> data) {
    constexpr size_t kChunk = 32;
    if (data.size() < kChunk) {
        return 1.0;
    }

    const size_t maxBytes = std::min<size_t>(data.size(), 256 * 1024);
    const size_t chunks = maxBytes / kChunk;
    if (chunks == 0) {
        return 1.0;
    }

    std::unordered_map<uint64_t, size_t> hashes;
    hashes.reserve(chunks);

    for (size_t offset = 0; offset + kChunk <= maxBytes; offset += kChunk) {
        uint64_t hash = 1469598103934665603ULL; // FNV offset basis
        for (size_t i = 0; i < kChunk; ++i) {
            hash ^= static_cast<uint64_t>(data[offset + i]);
            hash *= 1099511628211ULL;
        }
        ++hashes[hash];
    }

    return static_cast<double>(hashes.size()) / static_cast<double>(chunks);
}

std::string FileAnalyzer::normalizeExtension(std::string_view filenameHint) {
    const size_t dot = filenameHint.find_last_of('.');
    if (dot == std::string_view::npos) {
        return {};
    }
    return toLower(std::string(filenameHint.substr(dot)));
}

bool FileAnalyzer::detectStructured(FileType type, std::string_view mimeType) {
    if (type == FileType::Text_Structured) {
        return true;
    }
    return startsWith(mimeType, "application/json") || startsWith(mimeType, "application/xml") ||
        startsWith(mimeType, "text/xml") || startsWith(mimeType, "text/csv");
}

} // namespace mosqueeze
