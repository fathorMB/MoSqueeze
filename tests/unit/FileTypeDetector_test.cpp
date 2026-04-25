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

    // TIFF-based RAW by magic bytes
    std::vector<uint8_t> nefData = {0x49, 0x49, 0x2A, 0x00, 0x08, 0x00, 0x00, 0x00};
    nefData.resize(128, 0);
    writeFile(testDir / "test.nef", nefData);
    classification = detector.detect(testDir / "test.nef");
    assert(classification.type == mosqueeze::FileType::Image_Raw);
    assert(classification.canRecompress);
    std::cout << "[PASS] NEF raw detection OK\n";

    // RAW extension fallback without TIFF magic payload.
    std::vector<uint8_t> marker(128, 0xAB);
    writeFile(testDir / "test.cr3", marker);
    classification = detector.detect(testDir / "test.cr3");
    assert(classification.type == mosqueeze::FileType::Image_Raw);
    std::cout << "[PASS] CR3 extension fallback OK\n";

    writeFile(testDir / "test.dng", marker);
    classification = detector.detect(testDir / "test.dng");
    assert(classification.type == mosqueeze::FileType::Image_Raw);
    std::cout << "[PASS] DNG extension fallback OK\n";

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

    // PDF magic bytes (%PDF)
    std::vector<uint8_t> pdfData = {0x25, 0x50, 0x44, 0x46, 0x2D, 0x31, 0x2E, 0x34};
    pdfData.resize(100, 0);
    writeFile(testDir / "test.pdf", pdfData);
    classification = detector.detect(testDir / "test.pdf");
    assert(classification.type == mosqueeze::FileType::Document_PDF);
    assert(classification.mimeType == "application/pdf");
    assert(classification.isCompressed);
    assert(classification.canRecompress);
    std::cout << "[PASS] PDF magic detection OK\n";

    // PDF extension fallback (no valid magic)
    std::vector<uint8_t> fakePdf(100, 0xAB);
    writeFile(testDir / "fake.pdf", fakePdf);
    classification = detector.detect(testDir / "fake.pdf");
    assert(classification.type == mosqueeze::FileType::Document_PDF);
    std::cout << "[PASS] PDF extension fallback OK\n";

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
