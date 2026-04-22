#include <mosqueeze/preprocessors/PngOptimizer.hpp>

#include <zlib.h>

#include <array>
#include <cstring>
#include <set>
#include <stdexcept>

namespace mosqueeze {
namespace {

constexpr std::array<uint8_t, 8> kPngSignature = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

const std::set<std::string> kMetadataChunks = {
    "tEXt", "zTXt", "iTXt", "eXIf", "iCCP", "sRGB", "tIME", "pHYs", "bKGD", "hIST", "sPLT", "gAMA", "cHRM"
};

} // namespace

PreprocessResult PngOptimizer::preprocess(
    std::istream& input,
    std::ostream& output,
    FileType fileType) {
    if (!canProcess(fileType)) {
        throw std::runtime_error("PngOptimizer cannot process this file type");
    }

    const std::vector<uint8_t> raw((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (!isPng(raw)) {
        throw std::runtime_error("Invalid PNG signature");
    }

    std::vector<Chunk> chunks;
    if (!parseChunks(raw, chunks)) {
        throw std::runtime_error("Invalid PNG chunk layout");
    }

    std::vector<Chunk> optimized;
    optimized.reserve(chunks.size());
    std::vector<uint8_t> idatCombined;
    bool hasIhdr = false;
    bool hasIend = false;

    for (const Chunk& chunk : chunks) {
        if (chunk.type == "IHDR") {
            hasIhdr = true;
            optimized.push_back(chunk);
            continue;
        }
        if (chunk.type == "IDAT") {
            idatCombined.insert(idatCombined.end(), chunk.data.begin(), chunk.data.end());
            continue;
        }
        if (chunk.type == "IEND") {
            hasIend = true;
            continue;
        }
        if (isMetadataChunk(chunk.type)) {
            continue;
        }
        optimized.push_back(chunk);
    }

    if (!hasIhdr || !hasIend || idatCombined.empty()) {
        throw std::runtime_error("PNG missing required chunks");
    }

    const std::vector<uint8_t> inflated = inflateZlib(idatCombined);
    const std::vector<uint8_t> recompressed = deflateZlib(inflated, Z_BEST_COMPRESSION);

    Chunk idat{};
    idat.type = "IDAT";
    idat.data = recompressed;
    optimized.push_back(std::move(idat));

    Chunk iend{};
    iend.type = "IEND";
    optimized.push_back(std::move(iend));

    std::vector<uint8_t> rebuilt;
    rebuilt.reserve(raw.size());
    rebuilt.insert(rebuilt.end(), kPngSignature.begin(), kPngSignature.end());
    for (const Chunk& chunk : optimized) {
        appendChunk(rebuilt, chunk);
    }

    output.write(reinterpret_cast<const char*>(rebuilt.data()), static_cast<std::streamsize>(rebuilt.size()));

    PreprocessResult result{};
    result.type = PreprocessorType::PngOptimizer;
    result.originalBytes = raw.size();
    result.processedBytes = rebuilt.size();
    return result;
}

void PngOptimizer::postprocess(
    std::istream& input,
    std::ostream& output,
    const PreprocessResult& result) {
    (void)result;
    output << input.rdbuf();
}

bool PngOptimizer::isPng(const std::vector<uint8_t>& bytes) {
    return bytes.size() >= kPngSignature.size() &&
        std::equal(kPngSignature.begin(), kPngSignature.end(), bytes.begin());
}

bool PngOptimizer::isMetadataChunk(const std::string& type) {
    return kMetadataChunks.find(type) != kMetadataChunks.end();
}

bool PngOptimizer::parseChunks(const std::vector<uint8_t>& bytes, std::vector<Chunk>& chunks) {
    if (!isPng(bytes)) {
        return false;
    }

    size_t offset = kPngSignature.size();
    while (offset + 12 <= bytes.size()) {
        const uint32_t length = readU32BE(bytes.data() + offset);
        offset += 4;
        if (offset + 4 > bytes.size()) {
            return false;
        }
        Chunk chunk{};
        chunk.type.assign(reinterpret_cast<const char*>(bytes.data() + offset), 4);
        offset += 4;

        if (offset + length + 4 > bytes.size()) {
            return false;
        }
        chunk.data.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                          bytes.begin() + static_cast<std::ptrdiff_t>(offset + length));
        offset += length;
        // Skip CRC (validated only for structural bounds).
        offset += 4;
        chunks.push_back(std::move(chunk));
        if (!chunks.empty() && chunks.back().type == "IEND") {
            return true;
        }
    }

    return false;
}

uint32_t PngOptimizer::readU32BE(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24U) |
        (static_cast<uint32_t>(p[1]) << 16U) |
        (static_cast<uint32_t>(p[2]) << 8U) |
        static_cast<uint32_t>(p[3]);
}

void PngOptimizer::writeU32BE(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24U) & 0xFFU));
    out.push_back(static_cast<uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<uint8_t>(value & 0xFFU));
}

void PngOptimizer::appendChunk(std::vector<uint8_t>& out, const Chunk& chunk) {
    writeU32BE(out, static_cast<uint32_t>(chunk.data.size()));
    const size_t typeOffset = out.size();
    out.insert(out.end(), chunk.type.begin(), chunk.type.end());
    out.insert(out.end(), chunk.data.begin(), chunk.data.end());

    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const Bytef*>(out.data() + typeOffset), 4);
    if (!chunk.data.empty()) {
        crc = crc32(crc, reinterpret_cast<const Bytef*>(chunk.data.data()), static_cast<uInt>(chunk.data.size()));
    }
    writeU32BE(out, static_cast<uint32_t>(crc));
}

std::vector<uint8_t> PngOptimizer::inflateZlib(const std::vector<uint8_t>& in) {
    if (in.empty()) {
        return {};
    }

    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(in.data()));
    stream.avail_in = static_cast<uInt>(in.size());

    if (inflateInit(&stream) != Z_OK) {
        throw std::runtime_error("inflateInit failed");
    }

    std::vector<uint8_t> out;
    std::array<uint8_t, 64 * 1024> buffer{};
    int rc = Z_OK;
    do {
        stream.next_out = buffer.data();
        stream.avail_out = static_cast<uInt>(buffer.size());
        rc = inflate(&stream, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END) {
            inflateEnd(&stream);
            throw std::runtime_error("inflate failed");
        }
        const size_t produced = buffer.size() - static_cast<size_t>(stream.avail_out);
        out.insert(out.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(produced));
    } while (rc != Z_STREAM_END);

    inflateEnd(&stream);
    return out;
}

std::vector<uint8_t> PngOptimizer::deflateZlib(const std::vector<uint8_t>& in, int level) {
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(in.data()));
    stream.avail_in = static_cast<uInt>(in.size());

    if (deflateInit(&stream, level) != Z_OK) {
        throw std::runtime_error("deflateInit failed");
    }

    std::vector<uint8_t> out;
    std::array<uint8_t, 64 * 1024> buffer{};
    int rc = Z_OK;
    do {
        stream.next_out = buffer.data();
        stream.avail_out = static_cast<uInt>(buffer.size());
        rc = deflate(&stream, Z_FINISH);
        if (rc != Z_OK && rc != Z_STREAM_END) {
            deflateEnd(&stream);
            throw std::runtime_error("deflate failed");
        }
        const size_t produced = buffer.size() - static_cast<size_t>(stream.avail_out);
        out.insert(out.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(produced));
    } while (rc != Z_STREAM_END);

    deflateEnd(&stream);
    return out;
}

} // namespace mosqueeze
