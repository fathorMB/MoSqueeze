#pragma once

#include <mosqueeze/Preprocessor.hpp>

#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace mosqueeze {

enum class PngEngine : uint8_t {
    LibPng = 0,
    Oxipng = 1
};

class PngOptimizer : public IPreprocessor {
public:
    explicit PngOptimizer(PngEngine engine = PngEngine::LibPng);
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
        return canProcess(fileType) ? 0.20 : 0.0;
    }

    void setEngine(PngEngine engine);
    PngEngine configuredEngine() const { return configuredEngine_; }
    PngEngine effectiveEngine() const { return effectiveEngine_; }
    bool usedFallback() const { return usedFallback_; }

    void setCompressionLevel(int level);
    int compressionLevel() const { return compressionLevel_; }

    void setStripMetadata(bool strip) { stripMetadata_ = strip; }
    bool stripMetadata() const { return stripMetadata_; }

    void setFilterSelection(bool allFilters) { allFilters_ = allFilters; }
    bool allFilters() const { return allFilters_; }

    static bool isOxipngAvailable();
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
    static std::string quoteForShell(const std::string& path);

    std::vector<uint8_t> preprocessWithLibPng(const std::vector<uint8_t>& raw) const;
    std::vector<uint8_t> preprocessWithOxipng(const std::vector<uint8_t>& raw) const;

    PngEngine configuredEngine_ = PngEngine::LibPng;
    PngEngine effectiveEngine_ = PngEngine::LibPng;
    bool usedFallback_ = false;
    int compressionLevel_ = 9;
    bool stripMetadata_ = true;
    bool allFilters_ = true;
};

} // namespace mosqueeze
