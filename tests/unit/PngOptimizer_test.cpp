#include <mosqueeze/PreprocessorSelector.hpp>
#include <mosqueeze/preprocessors/PngOptimizer.hpp>

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main() {
    using mosqueeze::FileType;
    using mosqueeze::PngEngine;
    using mosqueeze::PngOptimizer;
    using mosqueeze::PreprocessorSelector;
    using mosqueeze::PreprocessorType;

    PngOptimizer optimizer;
    assert(optimizer.type() == PreprocessorType::PngOptimizer);
    assert(optimizer.name() == "png-optimizer");
    assert(optimizer.canProcess(FileType::Image_PNG));
    assert(!optimizer.canProcess(FileType::Image_JPEG));

    const std::vector<uint8_t> minimalPng = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
        0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x04, 0x00, 0x00, 0x00, 0xB5, 0x1C, 0x0C,
        0x02, 0x00, 0x00, 0x00, 0x0B, 0x49, 0x44, 0x41,
        0x54, 0x78, 0xDA, 0x63, 0xFC, 0xFF, 0x1F, 0x00,
        0x03, 0x03, 0x02, 0x00, 0xEF, 0x97, 0xF1, 0x9F,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44,
        0xAE, 0x42, 0x60, 0x82
    };

    std::istringstream in(std::string(minimalPng.begin(), minimalPng.end()));
    std::ostringstream out;
    const auto result = optimizer.preprocess(in, out, FileType::Image_PNG);
    const std::string optimized = out.str();

    assert(result.type == PreprocessorType::PngOptimizer);
    assert(result.originalBytes == minimalPng.size());
    assert(result.processedBytes == optimized.size());
    assert(result.metadata.size() >= 2);
    assert(static_cast<uint8_t>(optimized[0]) == 0x89);
    assert(optimized[1] == 'P');
    assert(optimized[2] == 'N');
    assert(optimized[3] == 'G');

    PngOptimizer oxipngOptimizer(PngEngine::Oxipng);
    oxipngOptimizer.setCompressionLevel(3);
    std::istringstream in2(std::string(minimalPng.begin(), minimalPng.end()));
    std::ostringstream out2;
    const auto result2 = oxipngOptimizer.preprocess(in2, out2, FileType::Image_PNG);
    assert(result2.type == PreprocessorType::PngOptimizer);
    assert(out2.str().size() > 8);
    if (!PngOptimizer::isOxipngAvailable()) {
        assert(oxipngOptimizer.usedFallback());
        assert(oxipngOptimizer.effectiveEngine() == PngEngine::LibPng);
    }

    PreprocessorSelector selector;
    auto names = selector.listNames();
    bool hasPng = false;
    for (const auto& n : names) {
        if (n == "png-optimizer") {
            hasPng = true;
            break;
        }
    }
    assert(hasPng);

    std::cout << "[PASS] PngOptimizer_test\n";
    return 0;
}
