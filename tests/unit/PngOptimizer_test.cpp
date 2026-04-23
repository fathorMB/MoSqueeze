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
    using mosqueeze::PreprocessorType;
    using mosqueeze::PreprocessorSelector;

    PngOptimizer optimizer;
    assert(optimizer.type() == PreprocessorType::PngOptimizer);
    assert(optimizer.name() == "png-optimizer");
    assert(optimizer.canProcess(FileType::Image_PNG));
    assert(!optimizer.canProcess(FileType::Image_JPEG));
    assert(!optimizer.canProcess(FileType::Text_Structured));

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

    std::istringstream input(std::string(minimalPng.begin(), minimalPng.end()));
    std::ostringstream output;
    const auto result = optimizer.preprocess(input, output, FileType::Image_PNG);
    const std::string optimized = output.str();

    assert(result.type == PreprocessorType::PngOptimizer);
    assert(result.originalBytes == minimalPng.size());
    assert(result.processedBytes == optimized.size());
    assert(result.metadata.size() >= 2);
    assert(optimized.size() > 8);
    assert(static_cast<uint8_t>(optimized[0]) == 0x89);
    assert(optimized[1] == 'P');
    assert(optimized[2] == 'N');
    assert(optimized[3] == 'G');

    std::istringstream postIn(optimized);
    std::ostringstream postOut;
    optimizer.postprocess(postIn, postOut, result);
    assert(postOut.str() == optimized);

    PngOptimizer oxipngOptimizer(PngEngine::Oxipng);
    oxipngOptimizer.setCompressionLevel(3);
    std::istringstream input2(std::string(minimalPng.begin(), minimalPng.end()));
    std::ostringstream output2;
    const auto result2 = oxipngOptimizer.preprocess(input2, output2, FileType::Image_PNG);
    assert(result2.type == PreprocessorType::PngOptimizer);
    assert(output2.str().size() > 8);
    if (!PngOptimizer::isOxipngAvailable()) {
        assert(oxipngOptimizer.usedFallback());
        assert(oxipngOptimizer.effectiveEngine() == PngEngine::LibPng);
    }

    PreprocessorSelector selector;
    auto* bestForPng = selector.selectBest(FileType::Image_PNG);
    assert(bestForPng != nullptr);
    assert(bestForPng->name() == "png-optimizer");

    std::cout << "[PASS] PngOptimizer_test\n";
    return 0;
}
