#include <mosqueeze/preprocessors/DictionaryPreprocessor.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

int main() {
    namespace fs = std::filesystem;
    const fs::path tempDir = fs::temp_directory_path() / "mosqueeze_dict_test";

    mosqueeze::DictionaryPreprocessor dict;
    assert(!dict.canProcess(mosqueeze::FileType::Text_Log));

    bool emptySamplesThrown = false;
    try {
        dict.trainFromSamples({}, 1024);
    } catch (const std::exception&) {
        emptySamplesThrown = true;
    }
    assert(emptySamplesThrown);

    bool zeroSizeThrown = false;
    try {
        dict.trainFromSamples({tempDir / "missing"}, 0);
    } catch (const std::exception&) {
        zeroSizeThrown = true;
    }
    assert(zeroSizeThrown);

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
    assert(dict.estimatedImprovement(mosqueeze::FileType::Text_Log) > 0.0);

    const std::string payload = "INFO service=api path=/v1/items id=999 status=200\n";
    std::istringstream in(payload);
    std::ostringstream processed;
    auto result = dict.preprocess(in, processed, mosqueeze::FileType::Text_Log);
    assert(result.originalBytes == payload.size());

    std::istringstream processedIn(processed.str());
    std::ostringstream restored;
    dict.postprocess(processedIn, restored, result);
    assert(restored.str() == payload);

    const fs::path dictPath = tempDir / "test.dict";
    dict.saveDictionary(dictPath);
    assert(fs::exists(dictPath));

    mosqueeze::DictionaryPreprocessor loaded;
    loaded.loadDictionary(dictPath);
    assert(loaded.canProcess(mosqueeze::FileType::Binary_Chunked));
    std::istringstream loadedIn(payload);
    std::ostringstream loadedOut;
    const auto loadedResult = loaded.preprocess(loadedIn, loadedOut, mosqueeze::FileType::Text_Log);
    assert(loadedResult.type == mosqueeze::PreprocessorType::DictionaryPreprocessor);
    assert(loadedOut.str() == payload);

    mosqueeze::DictionaryPreprocessor noDict;
    bool preprocessWithoutDictionaryThrown = false;
    try {
        std::istringstream inNoDict(payload);
        std::ostringstream outNoDict;
        (void)noDict.preprocess(inNoDict, outNoDict, mosqueeze::FileType::Text_Log);
    } catch (const std::exception&) {
        preprocessWithoutDictionaryThrown = true;
    }
    assert(preprocessWithoutDictionaryThrown);

    const fs::path emptySample = tempDir / "empty_sample.bin";
    {
        std::ofstream out(emptySample, std::ios::binary);
    }
    bool emptyTrainingDataThrown = false;
    try {
        mosqueeze::DictionaryPreprocessor emptyDict;
        emptyDict.trainFromSamples({emptySample}, 1024);
    } catch (const std::exception&) {
        emptyTrainingDataThrown = true;
    }
    assert(emptyTrainingDataThrown);

    fs::remove_all(tempDir);
    std::cout << "[PASS] DictionaryPreprocessor_test\n";
    return 0;
}
