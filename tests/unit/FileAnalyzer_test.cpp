#include <mosqueeze/FileAnalyzer.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main() {
    mosqueeze::FileAnalyzer analyzer;

    const std::vector<uint8_t> pngBytes = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
        0x00, 0x00, 0x00, 0x0D, 'I',  'H',  'D',  'R',
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01};
    const auto pngFeatures = analyzer.analyze(pngBytes, "sample.png");
    assert(pngFeatures.detectedType == "image/png");
    assert(pngFeatures.extension == ".png");
    assert(pngFeatures.fileSize == pngBytes.size());
    assert(pngFeatures.entropy >= 0.0);
    assert(pngFeatures.chunkRatio >= 0.0);
    assert(pngFeatures.chunkRatio <= 1.0);

    const std::string json = R"({"project":"mosqueeze","items":[1,2,3]})";
    const std::vector<uint8_t> jsonBytes(json.begin(), json.end());
    const auto jsonFeatures = analyzer.analyze(jsonBytes, "config.json");
    assert(jsonFeatures.detectedType == "application/json");
    assert(jsonFeatures.isStructured);
    assert(jsonFeatures.extension == ".json");
    assert(jsonFeatures.entropy > 1.0);
    assert(jsonFeatures.repeatPatterns > 0);

    const std::vector<uint8_t> rafBytes = {
        'F', 'U', 'J', 'I', 'F', 'I', 'L', 'M',
        0,   0,   0,   0,   0,   0,   0,   0};
    auto rawBuffer = rafBytes;
    rawBuffer.resize(96, 0);
    rawBuffer[84] = 1; // lossless compressed marker
    const auto rawFeatures = analyzer.analyze(rawBuffer, "photo.raf");
    assert(rawFeatures.rawFormat.has_value());
    assert(rawFeatures.rawFormat->manufacturer == "Fujifilm");

    std::cout << "[PASS] FileAnalyzer_test\n";
    return 0;
}
