#include <mosqueeze/preprocessors/DictionaryPreprocessor.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main() {
    namespace fs = std::filesystem;

    mosqueeze::DictionaryPreprocessor dict;

    const fs::path tempDir = fs::temp_directory_path() / "mosqueeze_dict_test";
    fs::create_directories(tempDir);
    std::vector<fs::path> samples;
    for (int i = 0; i < 120; ++i) {
        const fs::path samplePath = tempDir / ("sample_" + std::to_string(i) + ".txt");
        std::ofstream out(samplePath, std::ios::binary);
        out << "INFO service=api path=/v1/items id=" << i << " status=200\n";
        out << "INFO service=api path=/v1/items id=" << i << " status=200\n";
        samples.push_back(samplePath);
    }

    dict.trainFromSamples(samples, 8 * 1024);
    assert(dict.canProcess(mosqueeze::FileType::Text_Log));

    const std::string payload = "INFO service=api path=/v1/items id=999 status=200\n";
    std::istringstream in(payload);
    std::ostringstream processed;
    auto result = dict.preprocess(in, processed, mosqueeze::FileType::Text_Log);
    assert(result.originalBytes == payload.size());

    std::istringstream processedIn(processed.str());
    std::ostringstream restored;
    dict.postprocess(processedIn, restored, result);
    assert(restored.str() == payload);

    fs::remove_all(tempDir);
    std::cout << "[PASS] DictionaryPreprocessor_test\n";
    return 0;
}
