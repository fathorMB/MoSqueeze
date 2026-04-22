#include <mosqueeze/preprocessors/XmlCanonicalizer.hpp>

#include <cassert>
#include <iostream>
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

    std::cout << "[PASS] XmlCanonicalizer_test\n";
    return 0;
}
