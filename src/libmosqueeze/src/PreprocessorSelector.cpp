#include <mosqueeze/PreprocessorSelector.hpp>

#include <mosqueeze/preprocessors/DictionaryPreprocessor.hpp>
#include <mosqueeze/preprocessors/ImageMetaStripper.hpp>
#include <mosqueeze/preprocessors/JsonCanonicalizer.hpp>
#include <mosqueeze/preprocessors/XmlCanonicalizer.hpp>

namespace mosqueeze {

PreprocessorSelector::PreprocessorSelector() {
    registerPreprocessor(std::make_unique<JsonCanonicalizer>());
    registerPreprocessor(std::make_unique<XmlCanonicalizer>());
    registerPreprocessor(std::make_unique<ImageMetaStripper>());
    registerPreprocessor(std::make_unique<DictionaryPreprocessor>());
}

void PreprocessorSelector::registerPreprocessor(std::unique_ptr<IPreprocessor> preprocessor) {
    if (preprocessor) {
        preprocessors_.push_back(std::move(preprocessor));
    }
}

IPreprocessor* PreprocessorSelector::selectBest(FileType fileType) const {
    IPreprocessor* best = nullptr;
    double bestImprovement = -1.0;
    for (const auto& preprocessor : preprocessors_) {
        if (!preprocessor->canProcess(fileType)) {
            continue;
        }
        const double improvement = preprocessor->estimatedImprovement(fileType);
        if (improvement > bestImprovement) {
            bestImprovement = improvement;
            best = preprocessor.get();
        }
    }
    return best;
}

std::vector<IPreprocessor*> PreprocessorSelector::getApplicable(FileType fileType) const {
    std::vector<IPreprocessor*> out;
    for (const auto& preprocessor : preprocessors_) {
        if (preprocessor->canProcess(fileType)) {
            out.push_back(preprocessor.get());
        }
    }
    return out;
}

std::vector<IPreprocessor*> PreprocessorSelector::selectChain(FileType fileType) const {
    return getApplicable(fileType);
}

std::vector<std::string> PreprocessorSelector::listNames() const {
    std::vector<std::string> names;
    names.reserve(preprocessors_.size());
    for (const auto& preprocessor : preprocessors_) {
        names.push_back(preprocessor->name());
    }
    return names;
}

} // namespace mosqueeze
