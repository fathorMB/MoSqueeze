#include <mosqueeze/IntelligentSelector.hpp>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main() {
    mosqueeze::IntelligentSelector selector;

    const std::string json = R"({"k":"v","a":[1,2,3],"nested":{"x":1}})";
    const std::vector<uint8_t> jsonBytes(json.begin(), json.end());
    const auto jsonRec = selector.analyze(jsonBytes, "sample.json");
    assert(jsonRec.preprocessor == "json-canonical");
    assert(jsonRec.algorithm == "brotli");
    assert(jsonRec.level == 11);
    assert(jsonRec.confidence >= 0.6);
    assert(jsonRec.expectedRatio < 0.5);

    std::vector<uint8_t> randomBytes(4096);
    for (size_t i = 0; i < randomBytes.size(); ++i) {
        randomBytes[i] = static_cast<uint8_t>((i * 131U + 17U) & 0xFFU);
    }
    const auto unknownRec = selector.analyze(randomBytes, "unknown.bin");
    assert(!unknownRec.algorithm.empty());
    assert(!unknownRec.preprocessor.empty());
    assert(unknownRec.expectedRatio > 0.0);
    assert(unknownRec.expectedRatio <= 1.0);
    assert(unknownRec.confidence >= 0.0);
    assert(unknownRec.confidence <= 1.0);

    mosqueeze::IntelligentSelector fastestSelector(mosqueeze::OptimizationGoal::Fastest);
    const auto fastRec = fastestSelector.analyze(jsonBytes, "sample.json");
    assert(fastRec.level <= 11);
    assert(fastRec.expectedTimeMs <= jsonRec.expectedTimeMs);

    std::cout << "[PASS] IntelligentSelector_test\n";
    return 0;
}
