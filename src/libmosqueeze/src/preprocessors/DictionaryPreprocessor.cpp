#include <mosqueeze/preprocessors/DictionaryPreprocessor.hpp>

#include <zdict.h>

#include <fstream>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <string>

namespace mosqueeze {

bool DictionaryPreprocessor::canProcess(FileType fileType) const {
    (void)fileType;
    return hasDictionary_;
}

void DictionaryPreprocessor::trainFromSamples(
    const std::vector<std::filesystem::path>& samples,
    size_t dictSize) {
    if (samples.empty()) {
        throw std::runtime_error("Dictionary training requires at least one sample");
    }
    if (dictSize == 0) {
        throw std::runtime_error("Dictionary size must be > 0");
    }

    std::vector<std::string> loaded;
    loaded.reserve(samples.size());
    for (const auto& sample : samples) {
        std::ifstream in(sample, std::ios::binary);
        if (!in) {
            throw std::runtime_error("Failed to open training sample: " + sample.string());
        }
        loaded.emplace_back(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    std::vector<size_t> sampleSizes;
    sampleSizes.reserve(loaded.size());
    size_t totalBytes = 0;
    for (const auto& sample : loaded) {
        sampleSizes.push_back(sample.size());
        totalBytes += sample.size();
    }

    if (totalBytes == 0) {
        throw std::runtime_error("Dictionary training samples are empty");
    }

    std::vector<char> joined;
    joined.reserve(totalBytes);
    for (const auto& sample : loaded) {
        joined.insert(joined.end(), sample.begin(), sample.end());
    }

    dictionary_.assign(dictSize, 0);
    const size_t trained = ZDICT_trainFromBuffer(
        dictionary_.data(),
        dictionary_.size(),
        joined.data(),
        sampleSizes.data(),
        static_cast<unsigned>(sampleSizes.size()));

    if (ZDICT_isError(trained) != 0) {
        throw std::runtime_error("ZDICT_trainFromBuffer failed");
    }
    dictionary_.resize(trained);
    hasDictionary_ = !dictionary_.empty();
}

void DictionaryPreprocessor::saveDictionary(const std::filesystem::path& path) const {
    if (!hasDictionary_) {
        throw std::runtime_error("No dictionary to save");
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open dictionary output path: " + path.string());
    }
    out.write(reinterpret_cast<const char*>(dictionary_.data()), static_cast<std::streamsize>(dictionary_.size()));
}

void DictionaryPreprocessor::loadDictionary(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open dictionary path: " + path.string());
    }
    dictionary_.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    hasDictionary_ = !dictionary_.empty();
}

PreprocessResult DictionaryPreprocessor::preprocess(
    std::istream& input,
    std::ostream& output,
    FileType fileType) {
    if (!canProcess(fileType)) {
        throw std::runtime_error("DictionaryPreprocessor has no trained dictionary");
    }

    std::string raw((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    output.write(raw.data(), static_cast<std::streamsize>(raw.size()));

    PreprocessResult result{};
    result.type = PreprocessorType::DictionaryPreprocessor;
    result.originalBytes = raw.size();
    result.processedBytes = raw.size();
    return result;
}

void DictionaryPreprocessor::postprocess(
    std::istream& input,
    std::ostream& output,
    const PreprocessResult& result) {
    (void)result;
    output << input.rdbuf();
}

double DictionaryPreprocessor::estimatedImprovement(FileType fileType) const {
    (void)fileType;
    return hasDictionary_ ? 0.20 : 0.0;
}

} // namespace mosqueeze
