#include <mosqueeze/CompressionPipeline.hpp>
#include <mosqueeze/engines/ZstdEngine.hpp>
#include <mosqueeze/preprocessors/JsonCanonicalizer.hpp>

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    mosqueeze::ZstdEngine engine;
    mosqueeze::JsonCanonicalizer canon;
    mosqueeze::CompressionPipeline pipeline(&engine);
    pipeline.setPreprocessor(&canon);

    const std::string original = R"({"timestamp":"2024-01-15T10:30:00Z","user":"alice","items":[1,2,3]})";

    std::istringstream in(original);
    std::ostringstream compressed;
    auto pipelineResult = pipeline.compress(in, compressed, {});
    assert(pipelineResult.wasPreprocessed);
    assert(!compressed.str().empty());

    std::istringstream compressedIn(compressed.str());
    std::ostringstream restored;
    pipeline.decompress(compressedIn, restored);
    assert(restored.str() == original);

    std::cout << "[PASS] Pipeline_test\n";
    return 0;
}
