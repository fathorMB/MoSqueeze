#pragma once

#include <mosqueeze/Preprocessor.hpp>

#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace mosqueeze {

class PngOptimizer : public IPreprocessor {
public:
    PreprocessorType type() const override { return PreprocessorType::PngOptimizer; }
    std::string name() const override { return "png-optimizer"; }
    bool canProcess(FileType fileType) const override { return fileType == FileType::Image_PNG; }

    PreprocessResult preprocess(
        std::istream& input,
        std::ostream& output,
        FileType fileType) override;

    void postprocess(
        std::istream& input,
        std::ostream& output,
        const PreprocessResult& result) override;

    double estimatedImprovement(FileType fileType) const override {
        return canProcess(fileType) ? 0.15 : 0.0;
    }

private:
    struct Chunk {
        std::string type;
        std::vector<uint8_t> data;
    };

    static bool isPng(const std::vector<uint8_t>& bytes);
    static bool isMetadataChunk(const std::string& type);
    static bool parseChunks(const std::vector<uint8_t>& bytes, std::vector<Chunk>& chunks);
    static uint32_t readU32BE(const uint8_t* p);
    static void writeU32BE(std::vector<uint8_t>& out, uint32_t value);
    static void appendChunk(std::vector<uint8_t>& out, const Chunk& chunk);
    static std::vector<uint8_t> inflateZlib(const std::vector<uint8_t>& in);
    static std::vector<uint8_t> deflateZlib(const std::vector<uint8_t>& in, int level);
};

} // namespace mosqueeze
