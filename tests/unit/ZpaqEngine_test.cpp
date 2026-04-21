#include <mosqueeze/engines/ZpaqEngine.hpp>

#include <cassert>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    mosqueeze::ZpaqEngine engine;

    std::string original = "Hello, ZPAQ! Testing with minimal data. ";
    original += original;
    original += original;
    original += original;

    std::istringstream input(original);
    std::ostringstream compressed;

    std::cout << "[INFO] Compressing " << original.size() << " bytes with ZPAQ...\n";

    const auto start = std::chrono::steady_clock::now();
    const auto result = engine.compress(input, compressed);
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start);

    std::cout << "[INFO] Compression time: " << elapsed.count() << "s\n";
    std::cout << "[INFO] Original: " << result.originalBytes << " bytes\n";
    std::cout << "[INFO] Compressed: " << result.compressedBytes << " bytes\n";
    std::cout << "[INFO] Ratio: " << result.ratio() << "x\n";

    assert(result.ratio() >= 1.0);

    std::istringstream compressedInput(compressed.str());
    std::ostringstream decompressed;
    engine.decompress(compressedInput, decompressed);

    assert(decompressed.str() == original);

    const auto levels = engine.supportedLevels();
    assert(levels.size() == 5);
    assert(engine.name() == "zpaq");
    assert(engine.fileExtension() == ".zpaq");
    assert(engine.defaultLevel() == 5);
    assert(engine.maxLevel() == 5);

    std::cout << "[PASS] ZpaqEngine roundtrip OK\n";
    return 0;
}
