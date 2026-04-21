#include <mosqueeze/engines/BrotliEngine.hpp>

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    mosqueeze::BrotliEngine engine;

    std::string original = R"(
{
    "name": "test",
    "data": [1, 2, 3, 4, 5],
    "nested": {"key": "value"}
}
)";

    std::string seed = original;
    for (int i = 0; i < 12; ++i) {
        original += seed;
    }

    std::istringstream input(original);
    std::ostringstream compressed;

    auto result = engine.compress(input, compressed);
    assert(result.ratio() > 1.0);
    assert(result.compressedBytes < result.originalBytes);

    std::istringstream compressedInput(compressed.str());
    std::ostringstream decompressed;
    engine.decompress(compressedInput, decompressed);

    assert(decompressed.str() == original);

    auto levels = engine.supportedLevels();
    assert(levels.size() == 12);
    assert(engine.name() == "brotli");
    assert(engine.fileExtension() == ".br");
    assert(engine.defaultLevel() == 11);

    std::cout << "[PASS] BrotliEngine roundtrip OK\n";
    return 0;
}
