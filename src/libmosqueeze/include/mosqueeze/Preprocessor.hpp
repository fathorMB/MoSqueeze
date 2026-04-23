#pragma once

#include <mosqueeze/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace mosqueeze {

enum class PreprocessorType : uint8_t {
    None = 0,
    JsonCanonicalizer = 1,
    XmlCanonicalizer = 2,
    ImageMetaStripper = 3,
    DictionaryPreprocessor = 4,
    BayerPreprocessor = 5,
    PngOptimizer = 6
};

struct PreprocessResult {
    size_t originalBytes = 0;
    size_t processedBytes = 0;
    PreprocessorType type = PreprocessorType::None;
    std::vector<uint8_t> metadata;
};

class IPreprocessor {
public:
    virtual ~IPreprocessor() = default;

    virtual PreprocessorType type() const = 0;
    virtual std::string name() const = 0;
    virtual bool canProcess(FileType fileType) const = 0;

    virtual PreprocessResult preprocess(
        std::istream& input,
        std::ostream& output,
        FileType fileType) = 0;

    virtual void postprocess(
        std::istream& input,
        std::ostream& output,
        const PreprocessResult& result) = 0;

    virtual double estimatedImprovement(FileType fileType) const = 0;
};

} // namespace mosqueeze
