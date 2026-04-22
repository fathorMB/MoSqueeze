#pragma once

#include <mosqueeze/Preprocessor.hpp>

#include <filesystem>

namespace mosqueeze {

class DictionaryPreprocessor : public IPreprocessor {
public:
    PreprocessorType type() const override { return PreprocessorType::DictionaryPreprocessor; }
    std::string name() const override { return "zstd-dict"; }
    bool canProcess(FileType fileType) const override;

    void trainFromSamples(
        const std::vector<std::filesystem::path>& samples,
        size_t dictSize = 100 * 1024);
    void saveDictionary(const std::filesystem::path& path) const;
    void loadDictionary(const std::filesystem::path& path);

    PreprocessResult preprocess(
        std::istream& input,
        std::ostream& output,
        FileType fileType) override;

    void postprocess(
        std::istream& input,
        std::ostream& output,
        const PreprocessResult& result) override;

    double estimatedImprovement(FileType fileType) const override;

private:
    std::vector<uint8_t> dictionary_;
    bool hasDictionary_ = false;
};

} // namespace mosqueeze
