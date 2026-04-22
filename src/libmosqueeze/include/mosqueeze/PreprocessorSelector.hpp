#pragma once

#include <mosqueeze/Preprocessor.hpp>

#include <memory>
#include <string>
#include <vector>

namespace mosqueeze {

class PreprocessorSelector {
public:
    PreprocessorSelector();

    void registerPreprocessor(std::unique_ptr<IPreprocessor> preprocessor);
    IPreprocessor* selectBest(FileType fileType) const;
    std::vector<IPreprocessor*> getApplicable(FileType fileType) const;
    std::vector<IPreprocessor*> selectChain(FileType fileType) const;
    std::vector<std::string> listNames() const;

private:
    std::vector<std::unique_ptr<IPreprocessor>> preprocessors_;
};

} // namespace mosqueeze
