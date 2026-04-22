#include <mosqueeze/preprocessors/BayerPreprocessor.hpp>

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main() {
    mosqueeze::BayerPreprocessor preprocessor;
    assert(preprocessor.canProcess(mosqueeze::FileType::Image_Raw));
    assert(!preprocessor.canProcess(mosqueeze::FileType::Image_JPEG));

    // TIFF-like payload with little-endian header followed by 16-bit samples.
    std::vector<uint8_t> raw = {
        0x49, 0x49, 0x2A, 0x00, 0x08, 0x00, 0x00, 0x00
    };
    for (int i = 0; i < 32; ++i) {
        raw.push_back(static_cast<uint8_t>((i * 7) & 0xFF));
    }
    const std::string original(reinterpret_cast<const char*>(raw.data()), raw.size());

    std::istringstream in(original);
    std::ostringstream transformed;
    const auto result = preprocessor.preprocess(in, transformed, mosqueeze::FileType::Image_Raw);
    assert(result.type == mosqueeze::PreprocessorType::BayerPreprocessor);
    assert(result.originalBytes == original.size());
    assert(result.processedBytes == original.size());
    assert(result.metadata.size() >= 5);
    assert(transformed.str().size() == original.size());
    assert(transformed.str() != original);

    std::istringstream transformedIn(transformed.str());
    std::ostringstream restored;
    preprocessor.postprocess(transformedIn, restored, result);
    assert(restored.str() == original);

    const auto pattern = mosqueeze::BayerPreprocessor::detectPattern(raw);
    assert(pattern == mosqueeze::BayerPattern::RGGB);
    const auto meta = mosqueeze::BayerPreprocessor::extractMetadata(raw);
    assert(meta.has_value());
    assert(meta->pattern == mosqueeze::BayerPattern::RGGB);

    std::cout << "[PASS] BayerPreprocessor_test\n";
    return 0;
}
