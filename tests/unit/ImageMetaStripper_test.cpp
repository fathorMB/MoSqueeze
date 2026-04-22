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

    std::cout << "[PASS] ImageMetaStripper_test\n";
    return 0;
}
