#include <mosqueeze/preprocessors/ImageMetaStripper.hpp>

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    mosqueeze::ImageMetaStripper stripper;

    const std::string jpegLike = "\xFF\xD8\xFF\xE1\x00\x10META\xFF\xD9";
    std::istringstream in(jpegLike);
    std::ostringstream stripped;
    const auto result = stripper.preprocess(in, stripped, mosqueeze::FileType::Image_JPEG);
    assert(result.originalBytes == jpegLike.size());

    std::istringstream stripIn(stripped.str());
    std::ostringstream restored;
    stripper.postprocess(stripIn, restored, result);
    assert(restored.str() == jpegLike);
    assert(stripper.canProcess(mosqueeze::FileType::Image_Raw));

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

    std::cout << "[PASS] ImageMetaStripper_test\n";
    return 0;
}
