#include <mosqueeze/FileTypeDetector.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

void writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

} // namespace

int main() {
    const std::filesystem::path testDir = "test_files";
    std::filesystem::create_directory(testDir);

    mosqueeze::FileTypeDetector detector;

    // PNG magic bytes
    std::vector<uint8_t> pngData = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    pngData.resize(100, 0);
    writeFile(testDir / "test.png", pngData);

    auto classification = detector.detect(testDir / "test.png");
    assert(classification.type == mosqueeze::FileType::Image_PNG);
    assert(classification.isCompressed);
    std::cout << "[PASS] PNG detection OK\n";

    // JPEG magic bytes
    std::vector<uint8_t> jpegData = {0xFF, 0xD8, 0xFF};
    jpegData.resize(100, 0);
    writeFile(testDir / "test.jpg", jpegData);

    classification = detector.detect(testDir / "test.jpg");
    assert(classification.type == mosqueeze::FileType::Image_JPEG);
    assert(!classification.canRecompress);
    std::cout << "[PASS] JPEG detection OK\n";

    // JSON extension fallback
    writeFile(testDir / "data.json", {0x7B, 0x0A});
    classification = detector.detect(testDir / "data.json");
    assert(classification.type == mosqueeze::FileType::Text_Structured);
    std::cout << "[PASS] JSON detection OK\n";

    // MP4 by extension fallback
    writeFile(testDir / "video.mp4", {0x00, 0x00, 0x00, 0x18, 0x66, 0x74, 0x79, 0x70});
    classification = detector.detect(testDir / "video.mp4");
    assert(classification.isCompressed);
    assert(!classification.canRecompress);
    std::cout << "[PASS] MP4 detection OK\n";

    // Plain text sniffing
    const std::string text = "Hello world, this is plain text content.";
    std::vector<uint8_t> textData(text.begin(), text.end());
    writeFile(testDir / "plain.unknown", textData);
    classification = detector.detect(testDir / "plain.unknown");
    assert(classification.type == mosqueeze::FileType::Text_Prose);
    std::cout << "[PASS] Plain text detection OK\n";

    std::filesystem::remove_all(testDir);
    return 0;
}
