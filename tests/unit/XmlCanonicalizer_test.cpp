#include <mosqueeze/preprocessors/XmlCanonicalizer.hpp>

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>

int main() {
    mosqueeze::XmlCanonicalizer canon;
    const std::string input = R"(<?xml version="1.0"?><root> <a x="1" y="2">v</a></root>)";

    std::istringstream in(input);
    std::ostringstream preprocessed;
    const auto result = canon.preprocess(in, preprocessed, mosqueeze::FileType::Text_Structured);

    std::istringstream preIn(preprocessed.str());
    std::ostringstream restored;
    canon.postprocess(preIn, restored, result);
    assert(restored.str() == input);
    assert(!preprocessed.str().empty());
    assert(preprocessed.str().find("<root>") != std::string::npos);
    assert(preprocessed.str().find("</root>") != std::string::npos);
    assert(result.type == mosqueeze::PreprocessorType::XmlCanonicalizer);
    assert(result.metadata.size() == input.size());

    // postprocess fallback path when metadata is absent.
    mosqueeze::PreprocessResult emptyMeta{};
    std::istringstream passthroughIn("<x/>");
    std::ostringstream passthroughOut;
    canon.postprocess(passthroughIn, passthroughOut, emptyMeta);
    assert(passthroughOut.str() == "<x/>");

    // malformed XML must throw.
    bool malformedThrown = false;
    try {
        std::istringstream malformed("<root>");
        std::ostringstream ignored;
        (void)canon.preprocess(malformed, ignored, mosqueeze::FileType::Text_Structured);
    } catch (const std::exception&) {
        malformedThrown = true;
    }
    assert(malformedThrown);

    // wrong file type must throw.
    bool wrongTypeThrown = false;
    try {
        std::istringstream wrongType(input);
        std::ostringstream ignored;
        (void)canon.preprocess(wrongType, ignored, mosqueeze::FileType::Image_JPEG);
    } catch (const std::exception&) {
        wrongTypeThrown = true;
    }
    assert(wrongTypeThrown);

    std::cout << "[PASS] XmlCanonicalizer_test\n";
    return 0;
}
