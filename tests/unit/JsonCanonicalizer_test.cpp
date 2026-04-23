#include <mosqueeze/preprocessors/JsonCanonicalizer.hpp>

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>

int main() {
    mosqueeze::JsonCanonicalizer canon;
    const std::string input = R"({"z":1,"a":{"x":2,"b":3}})";
    const std::string equivalent = R"({ "a" : { "b" : 3, "x" : 2 }, "z" : 1 })";

    std::istringstream in(input);
    std::ostringstream preprocessed;
    const auto result = canon.preprocess(in, preprocessed, mosqueeze::FileType::Text_Structured);

    std::istringstream inEquivalent(equivalent);
    std::ostringstream preprocessedEquivalent;
    const auto resultEquivalent = canon.preprocess(inEquivalent, preprocessedEquivalent, mosqueeze::FileType::Text_Structured);

    std::istringstream preIn(preprocessed.str());
    std::ostringstream restored;
    canon.postprocess(preIn, restored, result);

    assert(restored.str() == input);
    assert(!preprocessed.str().empty());
    assert(preprocessedEquivalent.str() == preprocessed.str());
    assert(preprocessed.str().find("\"a\"") != std::string::npos);
    assert(preprocessed.str().find("\"z\"") != std::string::npos);
    assert(result.type == mosqueeze::PreprocessorType::JsonCanonicalizer);
    assert(result.metadata.size() == input.size());
    assert(resultEquivalent.type == mosqueeze::PreprocessorType::JsonCanonicalizer);

    // postprocess fallback path when metadata is absent.
    mosqueeze::PreprocessResult emptyMeta{};
    std::istringstream passthroughIn("{\"a\":1}");
    std::ostringstream passthroughOut;
    canon.postprocess(passthroughIn, passthroughOut, emptyMeta);
    assert(passthroughOut.str() == "{\"a\":1}");

    // malformed JSON must throw.
    bool malformedThrown = false;
    try {
        std::istringstream malformed(R"({"a":)");
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

    std::cout << "[PASS] JsonCanonicalizer_test\n";
    return 0;
}
