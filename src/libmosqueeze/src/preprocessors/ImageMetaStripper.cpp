#include <mosqueeze/preprocessors/ImageMetaStripper.hpp>

#include <iterator>
#include <stdexcept>
#include <string>

namespace mosqueeze {

PreprocessResult ImageMetaStripper::preprocess(
    std::istream& input,
    std::ostream& output,
    FileType fileType) {
    if (!canProcess(fileType)) {
        throw std::runtime_error("ImageMetaStripper cannot process this file type");
    }

    const std::string raw((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    // RAW/TIFF support is intentionally pass-through for now. Detection and
    // pipeline routing are enabled, while byte-level metadata stripping will
    // be added with a dedicated TIFF parser to preserve reversibility.
    (void)fileType;
    output.write(raw.data(), static_cast<std::streamsize>(raw.size()));

    PreprocessResult result{};
    result.type = PreprocessorType::ImageMetaStripper;
    result.originalBytes = raw.size();
    result.processedBytes = raw.size();
    return result;
}

void ImageMetaStripper::postprocess(
    std::istream& input,
    std::ostream& output,
    const PreprocessResult& result) {
    (void)result;
    output << input.rdbuf();
}

} // namespace mosqueeze
