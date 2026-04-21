#include <mosqueeze/engines/LzmaEngine.hpp>

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    mosqueeze::LzmaEngine engine;

    std::string original = "Hello, LZMA compression test. Binary data: ";
    for (int i = 0; i < 256; ++i) {
        original += static_cast<char>(i);
    }
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

    auto levels = engine.supportedLevels();
    assert(levels.size() == 10);
    assert(engine.name() == "xz");
    assert(engine.fileExtension() == ".xz");
    assert(engine.defaultLevel() == 9);

    std::cout << "[PASS] LzmaEngine roundtrip OK\n";
    return 0;
}
