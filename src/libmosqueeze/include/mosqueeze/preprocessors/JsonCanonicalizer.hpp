#pragma once

#include <mosqueeze/Preprocessor.hpp>

namespace mosqueeze {

class JsonCanonicalizer : public IPreprocessor {
public:
    PreprocessorType type() const override { return PreprocessorType::JsonCanonicalizer; }
    std::string name() const override { return "json-canonical"; }
    bool canProcess(FileType fileType) const override {
        return fileType == FileType::Text_Structured;
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
        return canProcess(fileType) ? 0.15 : 0.0;
    }
};

} // namespace mosqueeze
