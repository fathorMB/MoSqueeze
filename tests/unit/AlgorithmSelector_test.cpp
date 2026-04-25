#include <mosqueeze/AlgorithmSelector.hpp>
#include <mosqueeze/FileTypeDetector.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace mosqueeze;

void testTextSourceCode() {
    FileTypeDetector detector;
    AlgorithmSelector selector;

    std::ofstream f("test.cpp", std::ios::binary);
    f << "// Test source file\nint main() { return 0; }\n";
    f.close();

    const auto classification = detector.detect("test.cpp");
    const auto selection = selector.select(classification, "test.cpp");

    assert(!selection.shouldSkip);
    assert(selection.algorithm == "zstd");
    assert(selection.level == 22);
    assert(!selection.rationale.empty());

    std::filesystem::remove("test.cpp");
    std::cout << "[PASS] Text_SourceCode selection\n";
}

void testJsonFile() {
    FileClassification classification;
    classification.type = FileType::Text_Structured;
    classification.mimeType = "application/json";
    classification.isCompressed = false;
    classification.canRecompress = true;

    AlgorithmSelector selector;
    const auto selection = selector.select(classification);

    assert(!selection.shouldSkip);
    assert(selection.algorithm == "brotli");
    assert(selection.level == 11);

    std::cout << "[PASS] JSON selection\n";
}

void testSkipJpeg() {
    FileClassification classification;
    classification.type = FileType::Image_JPEG;
    classification.mimeType = "image/jpeg";
    classification.isCompressed = true;
    classification.canRecompress = false;
    classification.recommendation = "skip";

    AlgorithmSelector selector;
    const auto selection = selector.select(classification);

    assert(selection.shouldSkip);
    assert(selection.rationale.find("skip") != std::string::npos);

    std::cout << "[PASS] JPEG skip\n";
}

void testBinaryExecutable() {
    FileClassification classification;
    classification.type = FileType::Binary_Executable;
    classification.mimeType = "application/x-elf";
    classification.isCompressed = false;
    classification.canRecompress = true;

    AlgorithmSelector selector;
    const auto selection = selector.select(classification);

    assert(!selection.shouldSkip);
    assert(selection.algorithm == "lzma");
    assert(selection.level == 9);
    assert(selection.fallbackAlgorithm == "zstd");

    std::cout << "[PASS] Binary executable selection\n";
}

void testBinaryDatabase() {
    FileClassification classification;
    classification.type = FileType::Binary_Database;
    classification.mimeType = "application/vnd.sqlite3";
    classification.isCompressed = false;
    classification.canRecompress = true;

    AlgorithmSelector selector;
    const auto selection = selector.select(classification);

    assert(!selection.shouldSkip);
    assert(selection.algorithm == "zpaq");
    assert(selection.level == 5);
    assert(selection.fallbackAlgorithm == "lzma");

    std::cout << "[PASS] Binary database selection\n";
}

void testArchiveExtract() {
    FileClassification classification;
    classification.type = FileType::Archive_ZIP;
    classification.mimeType = "application/zip";
    classification.isCompressed = true;
    classification.canRecompress = false;
    classification.recommendation = "extract-then-compress";

    AlgorithmSelector selector;
    const auto selection = selector.select(classification);

    assert(selection.shouldSkip);
    assert(selection.rationale.find("extract") != std::string::npos);

    std::cout << "[PASS] Archive extract recommendation\n";
}

void testUnknownFile() {
    FileClassification classification;
    classification.type = FileType::Unknown;

    AlgorithmSelector selector;
    const auto selection = selector.select(classification);

    assert(!selection.shouldSkip);
    assert(selection.algorithm == "zstd");
    assert(selection.level == 22);

    std::cout << "[PASS] Unknown fallback\n";
}

void testJsonConfig() {
    AlgorithmSelector selector;

    assert(selector.saveRules("test_rules.json"));
    assert(selector.loadRules("test_rules.json"));

    std::filesystem::remove("test_rules.json");
    std::cout << "[PASS] JSON config save/load\n";
}

void testSkipFLAC() {
    FileClassification classification;
    classification.type = FileType::Audio_FLAC;
    classification.mimeType = "audio/flac";
    classification.extension = ".flac";
    classification.isCompressed = true;
    classification.canRecompress = false;
    classification.recommendation = "skip";

    AlgorithmSelector selector;
    const auto selection = selector.select(classification);

    assert(selection.shouldSkip);
    std::cout << "[PASS] FLAC skip\n";
}

void testSkipPDF() {
    FileClassification classification;
    classification.type = FileType::Document_PDF;
    classification.mimeType = "application/pdf";
    classification.extension = ".pdf";
    classification.isCompressed = true;
    classification.canRecompress = true;
    classification.recommendation = "compress";

    AlgorithmSelector selector;
    const auto selection = selector.select(classification);

    assert(selection.shouldSkip);
    std::cout << "[PASS] PDF skip\n";
}

void testEntropyThresholdSkip() {
    FileClassification classification;
    classification.type = FileType::Binary_Chunked;
    classification.mimeType = "application/octet-stream";
    classification.extension = ".bin";
    classification.isCompressed = false;
    classification.canRecompress = true;
    classification.recommendation = "compress";

    FileFeatures features;
    features.classification = classification;
    features.entropy = 7.8;

    SelectionConfig cfg;
    cfg.entropyThreshold = 7.5;
    cfg.skipExtensions = {".bin"};

    AlgorithmSelector selector;
    selector.setConfig(cfg);
    const auto selection = selector.select(classification, features);

    assert(selection.shouldSkip);
    std::cout << "[PASS] Entropy threshold skip\n";
}

void testEntropyThresholdPass() {
    FileClassification classification;
    classification.type = FileType::Binary_Chunked;
    classification.mimeType = "application/octet-stream";
    classification.extension = ".dat";
    classification.isCompressed = false;
    classification.canRecompress = true;
    classification.recommendation = "compress";

    FileFeatures features;
    features.classification = classification;
    features.entropy = 4.0;

    SelectionConfig cfg;
    cfg.entropyThreshold = 7.5;

    AlgorithmSelector selector;
    selector.setConfig(cfg);
    const auto selection = selector.select(classification, features);

    assert(!selection.shouldSkip);
    std::cout << "[PASS] Entropy threshold pass (low entropy compresses)\n";
}

void testTextStillCompresses() {
    FileClassification classification;
    classification.type = FileType::Text_Prose;
    classification.mimeType = "text/plain";
    classification.extension = ".txt";
    classification.isCompressed = false;
    classification.canRecompress = true;
    classification.recommendation = "compress";

    FileFeatures features;
    features.classification = classification;
    features.entropy = 4.05;

    AlgorithmSelector selector;
    const auto selection = selector.select(classification, features);

    assert(!selection.shouldSkip);
    assert(selection.algorithm == "brotli" || selection.algorithm == "zstd");
    std::cout << "[PASS] Text still compresses with low entropy\n";
}

int main() {
    testTextSourceCode();
    testJsonFile();
    testSkipJpeg();
    testBinaryExecutable();
    testBinaryDatabase();
    testArchiveExtract();
    testUnknownFile();
    testJsonConfig();
    testSkipFLAC();
    testSkipPDF();
    testEntropyThresholdSkip();
    testEntropyThresholdPass();
    testTextStillCompresses();

    std::cout << "[PASS] All AlgorithmSelector tests passed\n";
    return 0;
}
