#include <mosqueeze/preprocessors/JsonCanonicalizer.hpp>

#include <nlohmann/json.hpp>

#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

namespace mosqueeze {

PreprocessResult JsonCanonicalizer::preprocess(
    std::istream& input,
    std::ostream& output,
    FileType fileType) {
    if (!canProcess(fileType)) {
        throw std::runtime_error("JsonCanonicalizer cannot process this file type");
    }

    const std::string raw((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    nlohmann::json json = nlohmann::json::parse(raw);
    const std::string canonical = json.dump();
    output.write(canonical.data(), static_cast<std::streamsize>(canonical.size()));

    PreprocessResult result{};
    result.type = PreprocessorType::JsonCanonicalizer;
    result.originalBytes = raw.size();
    result.processedBytes = canonical.size();
    result.metadata.assign(raw.begin(), raw.end());
    return result;
}

void JsonCanonicalizer::postprocess(
    std::istream& input,
    std::ostream& output,
    const PreprocessResult& result) {
    if (!result.metadata.empty()) {
        output.write(
            reinterpret_cast<const char*>(result.metadata.data()),
            static_cast<std::streamsize>(result.metadata.size()));
        return;
    }

    std::ostringstream passthrough;
    passthrough << input.rdbuf();
    const std::string raw = passthrough.str();
    output.write(raw.data(), static_cast<std::streamsize>(raw.size()));
}

} // namespace mosqueeze
