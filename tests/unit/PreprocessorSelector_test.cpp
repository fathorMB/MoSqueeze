#include <mosqueeze/PreprocessorSelector.hpp>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool containsName(const std::vector<std::string>& names, const std::string& expected) {
    return std::find(names.begin(), names.end(), expected) != names.end();
}

bool containsType(const std::vector<mosqueeze::IPreprocessor*>& preprocessors, const std::string& expectedName) {
    return std::any_of(preprocessors.begin(), preprocessors.end(), [&](const auto* preprocessor) {
        return preprocessor != nullptr && preprocessor->name() == expectedName;
    });
}

} // namespace

int main() {
    mosqueeze::PreprocessorSelector selector;

    const auto names = selector.listNames();
    assert(names.size() == 6);
    assert(containsName(names, "json-canonical"));
    assert(containsName(names, "xml-canonical"));
    assert(containsName(names, "image-meta-strip"));
    assert(containsName(names, "png-optimizer"));
    assert(containsName(names, "bayer-raw"));
    assert(containsName(names, "zstd-dict"));

    const auto structuredApplicable = selector.getApplicable(mosqueeze::FileType::Text_Structured);
    assert(structuredApplicable.size() == 2);
    assert(containsType(structuredApplicable, "json-canonical"));
    assert(containsType(structuredApplicable, "xml-canonical"));

    const auto rawApplicable = selector.getApplicable(mosqueeze::FileType::Image_Raw);
    assert(rawApplicable.size() == 2);
    assert(containsType(rawApplicable, "bayer-raw"));
    assert(containsType(rawApplicable, "image-meta-strip"));

    const auto jpegApplicable = selector.selectChain(mosqueeze::FileType::Image_JPEG);
    assert(jpegApplicable.size() == 1);
    assert(jpegApplicable.front()->name() == "image-meta-strip");

    auto* bestStructured = selector.selectBest(mosqueeze::FileType::Text_Structured);
    assert(bestStructured != nullptr);
    assert(bestStructured->name() == "json-canonical");

    auto* bestRaw = selector.selectBest(mosqueeze::FileType::Image_Raw);
    assert(bestRaw != nullptr);
    assert(bestRaw->name() == "bayer-raw");

    auto* bestJpeg = selector.selectBest(mosqueeze::FileType::Image_JPEG);
    assert(bestJpeg != nullptr);
    assert(bestJpeg->name() == "image-meta-strip");

    auto* bestUnknown = selector.selectBest(mosqueeze::FileType::Unknown);
    assert(bestUnknown == nullptr);

    std::cout << "[PASS] PreprocessorSelector_test\n";
    return 0;
}
