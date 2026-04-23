#include <mosqueeze/preprocessors/ImageMetaStripper.hpp>

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>

int main() {
    mosqueeze::ImageMetaStripper stripper;
    assert(stripper.canProcess(mosqueeze::FileType::Image_JPEG));
    assert(stripper.canProcess(mosqueeze::FileType::Image_PNG));
    assert(stripper.canProcess(mosqueeze::FileType::Image_Raw));
    assert(!stripper.canProcess(mosqueeze::FileType::Text_Structured));

    const std::string jpegLike = "\xFF\xD8\xFF\xE1\x00\x10META\xFF\xD9";
    std::istringstream in(jpegLike);
    std::ostringstream stripped;
    const auto result = stripper.preprocess(in, stripped, mosqueeze::FileType::Image_JPEG);
    assert(result.originalBytes == jpegLike.size());

    std::istringstream stripIn(stripped.str());
    std::ostringstream restored;
    stripper.postprocess(stripIn, restored, result);
    assert(restored.str() == jpegLike);
    assert(result.type == mosqueeze::PreprocessorType::ImageMetaStripper);
    assert(result.processedBytes == jpegLike.size());

    const std::string tiffLike = "II*\x00\x08\x00\x00\x00rawdata";
    std::istringstream rawIn(tiffLike);
    std::ostringstream rawOut;
    const auto rawResult = stripper.preprocess(rawIn, rawOut, mosqueeze::FileType::Image_Raw);
    assert(rawResult.originalBytes == tiffLike.size());
    assert(rawResult.processedBytes == tiffLike.size());

    std::istringstream rawStripIn(rawOut.str());
    std::ostringstream rawRestored;
    stripper.postprocess(rawStripIn, rawRestored, rawResult);
    assert(rawRestored.str() == tiffLike);

    const std::string empty;
    std::istringstream emptyIn(empty);
    std::ostringstream emptyOut;
    const auto emptyResult = stripper.preprocess(emptyIn, emptyOut, mosqueeze::FileType::Image_PNG);
    assert(emptyResult.originalBytes == 0);
    assert(emptyResult.processedBytes == 0);
    assert(emptyOut.str().empty());

    bool wrongTypeThrown = false;
    try {
        std::istringstream wrongType("x");
        std::ostringstream ignored;
        (void)stripper.preprocess(wrongType, ignored, mosqueeze::FileType::Text_Prose);
    } catch (const std::exception&) {
        wrongTypeThrown = true;
    }
    assert(wrongTypeThrown);

    std::cout << "[PASS] ImageMetaStripper_test\n";
    return 0;
}
