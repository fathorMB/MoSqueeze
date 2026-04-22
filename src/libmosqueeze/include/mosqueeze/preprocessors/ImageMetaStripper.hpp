#pragma once

#include <mosqueeze/Preprocessor.hpp>

namespace mosqueeze {

class ImageMetaStripper : public IPreprocessor {
public:
    PreprocessorType type() const override { return PreprocessorType::ImageMetaStripper; }
    std::string name() const override { return "image-meta-strip"; }
    bool canProcess(FileType fileType) const override {
        return fileType == FileType::Image_JPEG ||
            fileType == FileType::Image_PNG ||
            fileType == FileType::Image_Raw;
    }

    PreprocessResult preprocess(
        std::istream& input,
        std::ostream& output,
        FileType fileType) override;

    void postprocess(
        std::istream& input,
        std::ostream& output,
        const PreprocessResult& result) override;

    double estimatedImprovement(FileType fileType) const override {
        if (fileType == FileType::Image_JPEG) {
            return 0.05;
        }
        if (fileType == FileType::Image_PNG) {
            return 0.02;
        }
        if (fileType == FileType::Image_Raw) {
            return 0.03;
        }
        return 0.0;
    }
};

} // namespace mosqueeze
