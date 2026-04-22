#include <mosqueeze/preprocessors/XmlCanonicalizer.hpp>

#include <pugixml.hpp>

#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

namespace mosqueeze {

PreprocessResult XmlCanonicalizer::preprocess(
    std::istream& input,
    std::ostream& output,
    FileType fileType) {
    if (!canProcess(fileType)) {
        throw std::runtime_error("XmlCanonicalizer cannot process this file type");
    }

    const std::string raw((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    pugi::xml_document doc;
    const pugi::xml_parse_result parseResult = doc.load_string(raw.c_str(), pugi::parse_default);
    if (!parseResult) {
        throw std::runtime_error("XmlCanonicalizer parse error");
    }

    std::ostringstream normalized;
    doc.save(normalized, "", pugi::format_raw | pugi::format_no_declaration);
    const std::string canonical = normalized.str();
    output.write(canonical.data(), static_cast<std::streamsize>(canonical.size()));

    PreprocessResult result{};
    result.type = PreprocessorType::XmlCanonicalizer;
    result.originalBytes = raw.size();
    result.processedBytes = canonical.size();
    result.metadata.assign(raw.begin(), raw.end());
    return result;
}

void XmlCanonicalizer::postprocess(
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
