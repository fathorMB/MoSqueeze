#include <mosqueeze/RawFormat.hpp>

#include <cassert>
#include <iostream>
#include <vector>

namespace {

std::vector<uint8_t> makeRaf(uint8_t compressionMarker) {
    std::vector<uint8_t> data(128, 0);
    const char* magic = "FUJIFILM";
    for (size_t i = 0; i < 8; ++i) {
        data[i] = static_cast<uint8_t>(magic[i]);
    }
    data[84] = compressionMarker;
    return data;
}

} // namespace

int main() {
    {
        const auto rafLossless = makeRaf(1);
        const auto detected = mosqueeze::RawFormatDetector::detect(rafLossless);
        assert(detected.has_value());
        assert(detected->extension == ".RAF");
        assert(detected->compression == mosqueeze::RawCompression::LosslessCompressed);
        assert(!mosqueeze::RawFormatDetector::shouldApplyBayerPreprocessor(detected->compression));
    }

    {
        const auto rafUncompressed = makeRaf(0);
        const auto detected = mosqueeze::RawFormatDetector::detect(rafUncompressed);
        assert(detected.has_value());
        assert(detected->compression == mosqueeze::RawCompression::Uncompressed);
        assert(mosqueeze::RawFormatDetector::shouldApplyBayerPreprocessor(detected->compression));
    }

    {
        const auto rafLossy = makeRaf(2);
        const auto detected = mosqueeze::RawFormatDetector::detect(rafLossy);
        assert(detected.has_value());
        assert(detected->compression == mosqueeze::RawCompression::LossyCompressed);
    }

    {
        std::vector<uint8_t> iiq = {'I', 'I', 'Q', 0x00, 0x01, 0x02, 0x03, 0x04};
        const auto detected = mosqueeze::RawFormatDetector::detect(iiq);
        assert(detected.has_value());
        assert(detected->extension == ".IIQ");
        assert(detected->compression == mosqueeze::RawCompression::Uncompressed);
    }

    {
        const auto byExt = mosqueeze::RawFormatDetector::detectByExtension("sample.3fr");
        assert(byExt.has_value());
        assert(byExt->extension == ".3FR");
        assert(byExt->compression == mosqueeze::RawCompression::Uncompressed);
    }

    {
        const auto byExt = mosqueeze::RawFormatDetector::detectByExtension("sample.nef");
        assert(byExt.has_value());
        assert(byExt->extension == ".NEF");
        assert(byExt->compression == mosqueeze::RawCompression::Unknown);
    }

    {
        std::vector<uint8_t> unknown = {0x00, 0x01, 0x02, 0x03};
        const auto detected = mosqueeze::RawFormatDetector::detect(unknown);
        assert(!detected.has_value());
    }

    std::cout << "[PASS] RawFormat_test\n";
    return 0;
}
