#include <mosqueeze/preprocessors/JsonCanonicalizer.hpp>

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    mosqueeze::JsonCanonicalizer canon;
    const std::string input = R"({"z":1,"a":{"x":2,"b":3}})";

    std::istringstream in(input);
    std::ostringstream preprocessed;
    const auto result = canon.preprocess(in, preprocessed, mosqueeze::FileType::Text_Structured);

    std::istringstream preIn(preprocessed.str());
    std::ostringstream restored;
    canon.postprocess(preIn, restored, result);

    assert(restored.str() == input);
    assert(!preprocessed.str().empty());
    assert(preprocessed.str().find("\"a\"") != std::string::npos);
    assert(preprocessed.str().find("\"z\"") != std::string::npos);

    std::cout << "[PASS] JsonCanonicalizer_test\n";
    return 0;
}
