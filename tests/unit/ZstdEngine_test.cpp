#include <mosqueeze/engines/ZstdEngine.hpp>

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    mosqueeze::ZstdEngine engine;

    // Roundtrip test
    std::string original = "Hello, this is a test string for compression. ";
    original += original;
    original += original;

    std::istringstream input(original);
    std::ostringstream compressed;

    auto result = engine.compress(input, compressed);
    assert(result.ratio() > 1.0);
    assert(result.compressedBytes < result.originalBytes);

    std::istringstream compressedInput(compressed.str());
    std::ostringstream decompressed;
    engine.decompress(compressedInput, decompressed);

    assert(decompressed.str() == original);

    // Metadata and levels
    auto levels = engine.supportedLevels();
    assert(!levels.empty());
    assert(engine.name() == "zstd");
    assert(engine.fileExtension() == ".zst");
    assert(engine.maxLevel() >= engine.defaultLevel());

    std::cout << "[PASS] ZstdEngine roundtrip OK\n";
    return 0;
}
