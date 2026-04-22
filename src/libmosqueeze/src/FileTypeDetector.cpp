#include <mosqueeze/FileTypeDetector.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <unordered_map>

namespace mosqueeze {
namespace {

FileClassification makeClassification(
    FileType type,
    std::string mime,
    bool isCompressed,
    bool canRecompress,
    const std::string& extension) {
    FileClassification classification{};
    classification.type = type;
    classification.mimeType = std::move(mime);
    classification.extension = extension;
    classification.isCompressed = isCompressed;
    classification.canRecompress = canRecompress;

    if (!canRecompress) {
        if (type == FileType::Archive_ZIP || type == FileType::Archive_7Z || type == FileType::Archive_TAR) {
            classification.recommendation = "extract-then-compress";
        } else {
            classification.recommendation = "skip";
        }
        return classification;
    }

    if (isCompressed && (type == FileType::Archive_ZIP || type == FileType::Archive_7Z || type == FileType::Archive_TAR)) {
        classification.recommendation = "extract-then-compress";
    } else {
        classification.recommendation = "compress";
    }

    return classification;
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool startsWith(const std::string& text, const std::string& prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

std::string trimLeft(const std::string& text) {
    size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
        ++start;
    }
    return text.substr(start);
}

bool isValidUtf8(const std::vector<uint8_t>& buffer) {
    size_t i = 0;
    while (i < buffer.size()) {
        const uint8_t c = buffer[i];
        if (c <= 0x7F) {
            ++i;
            continue;
        }

        size_t extra = 0;
        uint32_t codepoint = 0;

        if ((c & 0xE0U) == 0xC0U) {
            extra = 1;
            codepoint = c & 0x1FU;
            if (codepoint == 0) {
                return false;
            }
        } else if ((c & 0xF0U) == 0xE0U) {
            extra = 2;
            codepoint = c & 0x0FU;
        } else if ((c & 0xF8U) == 0xF0U) {
            extra = 3;
            codepoint = c & 0x07U;
            if (codepoint > 0x04U) {
                return false;
            }
        } else {
            return false;
        }

        if (i + extra >= buffer.size()) {
            return false;
        }

        for (size_t j = 1; j <= extra; ++j) {
            const uint8_t cc = buffer[i + j];
            if ((cc & 0xC0U) != 0x80U) {
                return false;
            }
            codepoint = (codepoint << 6U) | (cc & 0x3FU);
        }

        if ((extra == 1 && codepoint < 0x80U) ||
            (extra == 2 && codepoint < 0x800U) ||
            (extra == 3 && codepoint < 0x10000U) ||
            codepoint > 0x10FFFFU ||
            (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) {
            return false;
        }

        i += extra + 1;
    }

    return true;
}

} // namespace

FileTypeDetector::FileTypeDetector() {
    registerCommonTypes();
}

FileClassification FileTypeDetector::detect(const std::filesystem::path& path) {
    const std::string extension = toLower(path.extension().string());

    std::vector<uint8_t> buffer;
    buffer.resize(8192);

    std::ifstream in(path, std::ios::binary);
    if (in) {
        in.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        buffer.resize(static_cast<size_t>(in.gcount()));
    } else {
        return detectFromExtension(extension);
    }

    auto byMagic = detectFromMagic(buffer);
    if (byMagic.type != FileType::Unknown) {
        byMagic.extension = extension;
        return byMagic;
    }

    auto byExtension = detectFromExtension(extension);
    if (byExtension.type != FileType::Unknown) {
        byExtension.extension = extension;
        return byExtension;
    }

    auto byText = detectTextContent(buffer);
    byText.extension = extension;
    return byText;
}

FileClassification FileTypeDetector::detectFromMagic(const std::vector<uint8_t>& buffer) {
    if (buffer.size() >= 12 &&
        buffer[0] == 'R' && buffer[1] == 'I' && buffer[2] == 'F' && buffer[3] == 'F' &&
        buffer[8] == 'A' && buffer[9] == 'V' && buffer[10] == 'I') {
        return makeClassification(FileType::Video_AVI, "video/x-msvideo", false, true, ".avi");
    }

    if (buffer.size() >= 12 &&
        buffer[0] == 0x52 && buffer[1] == 0x49 && buffer[2] == 0x46 && buffer[3] == 0x46 &&
        buffer[8] == 0x57 && buffer[9] == 0x45 && buffer[10] == 0x42 && buffer[11] == 0x50) {
        return makeClassification(FileType::Image_WebP, "image/webp", true, false, "");
    }

    if (buffer.size() >= 8 &&
        buffer[4] == 0x66 && buffer[5] == 0x74 && buffer[6] == 0x79 && buffer[7] == 0x70) {
        return makeClassification(FileType::Video_MP4, "video/mp4", true, false, "");
    }

    for (const auto& entry : magicEntries_) {
        if (buffer.size() < entry.offset + entry.bytes.size()) {
            continue;
        }

        bool matches = true;
        for (size_t i = 0; i < entry.bytes.size(); ++i) {
            if (buffer[entry.offset + i] != entry.bytes[i]) {
                matches = false;
                break;
            }
        }

        if (matches) {
            return makeClassification(entry.type, entry.mime, entry.isCompressed, entry.canRecompress, "");
        }
    }

    return {};
}

FileClassification FileTypeDetector::detectFromExtension(const std::string& ext) {
    const std::string normalized = toLower(ext);

    static const std::unordered_map<std::string, FileClassification> extensionMap = {
        {".txt", makeClassification(FileType::Text_Prose, "text/plain", false, true, ".txt")},
        {".md", makeClassification(FileType::Text_Prose, "text/markdown", false, true, ".md")},
        {".rst", makeClassification(FileType::Text_Prose, "text/plain", false, true, ".rst")},

        {".c", makeClassification(FileType::Text_SourceCode, "text/x-c", false, true, ".c")},
        {".cc", makeClassification(FileType::Text_SourceCode, "text/x-c++", false, true, ".cc")},
        {".cpp", makeClassification(FileType::Text_SourceCode, "text/x-c++", false, true, ".cpp")},
        {".h", makeClassification(FileType::Text_SourceCode, "text/x-c", false, true, ".h")},
        {".hpp", makeClassification(FileType::Text_SourceCode, "text/x-c++", false, true, ".hpp")},
        {".cs", makeClassification(FileType::Text_SourceCode, "text/plain", false, true, ".cs")},
        {".py", makeClassification(FileType::Text_SourceCode, "text/x-python", false, true, ".py")},
        {".js", makeClassification(FileType::Text_SourceCode, "text/javascript", false, true, ".js")},
        {".ts", makeClassification(FileType::Text_SourceCode, "text/typescript", false, true, ".ts")},
        {".rs", makeClassification(FileType::Text_SourceCode, "text/plain", false, true, ".rs")},
        {".go", makeClassification(FileType::Text_SourceCode, "text/plain", false, true, ".go")},
        {".java", makeClassification(FileType::Text_SourceCode, "text/x-java", false, true, ".java")},
        {".css", makeClassification(FileType::Text_SourceCode, "text/css", false, true, ".css")},
        {".scss", makeClassification(FileType::Text_SourceCode, "text/x-scss", false, true, ".scss")},
        {".less", makeClassification(FileType::Text_SourceCode, "text/plain", false, true, ".less")},

        {".json", makeClassification(FileType::Text_Structured, "application/json", false, true, ".json")},
        {".xml", makeClassification(FileType::Text_Structured, "application/xml", false, true, ".xml")},
        {".yaml", makeClassification(FileType::Text_Structured, "application/yaml", false, true, ".yaml")},
        {".yml", makeClassification(FileType::Text_Structured, "application/yaml", false, true, ".yml")},
        {".toml", makeClassification(FileType::Text_Structured, "application/toml", false, true, ".toml")},
        {".csv", makeClassification(FileType::Text_Structured, "text/csv", false, true, ".csv")},
        {".sql", makeClassification(FileType::Text_Structured, "application/sql", false, true, ".sql")},
        {".html", makeClassification(FileType::Text_Structured, "text/html", false, true, ".html")},
        {".htm", makeClassification(FileType::Text_Structured, "text/html", false, true, ".htm")},

        {".log", makeClassification(FileType::Text_Log, "text/plain", false, true, ".log")},

        {".wav", makeClassification(FileType::Audio_WAV, "audio/wav", false, true, ".wav")},
        {".mp3", makeClassification(FileType::Audio_MP3, "audio/mpeg", true, false, ".mp3")},
        {".flac", makeClassification(FileType::Audio_FLAC, "audio/flac", true, false, ".flac")},
        {".aac", makeClassification(FileType::Audio_MP3, "audio/aac", true, false, ".aac")},
        {".ogg", makeClassification(FileType::Audio_MP3, "audio/ogg", true, false, ".ogg")},
        {".m4a", makeClassification(FileType::Audio_MP3, "audio/mp4", true, false, ".m4a")},

        {".mp4", makeClassification(FileType::Video_MP4, "video/mp4", true, false, ".mp4")},
        {".mkv", makeClassification(FileType::Video_MKV, "video/x-matroska", true, true, ".mkv")},
        {".webm", makeClassification(FileType::Video_WebM, "video/webm", true, false, ".webm")},
        {".avi", makeClassification(FileType::Video_AVI, "video/x-msvideo", false, true, ".avi")},
        {".mov", makeClassification(FileType::Video_MP4, "video/quicktime", true, false, ".mov")},

        {".tif", makeClassification(FileType::Image_Raw, "image/tiff", false, true, ".tif")},
        {".tiff", makeClassification(FileType::Image_Raw, "image/tiff", false, true, ".tiff")},
        {".nef", makeClassification(FileType::Image_Raw, "image/x-nikon-nef", false, true, ".nef")},
        {".nrw", makeClassification(FileType::Image_Raw, "image/x-nikon-nrw", false, true, ".nrw")},
        {".cr2", makeClassification(FileType::Image_Raw, "image/x-canon-cr2", false, true, ".cr2")},
        {".cr3", makeClassification(FileType::Image_Raw, "image/x-canon-cr3", false, true, ".cr3")},
        {".raf", makeClassification(FileType::Image_Raw, "image/x-fuji-raf", false, true, ".raf")},
        {".arw", makeClassification(FileType::Image_Raw, "image/x-sony-arw", false, true, ".arw")},
        {".sr2", makeClassification(FileType::Image_Raw, "image/x-sony-sr2", false, true, ".sr2")},
        {".dng", makeClassification(FileType::Image_Raw, "image/x-adobe-dng", false, true, ".dng")},
        {".orf", makeClassification(FileType::Image_Raw, "image/x-olympus-orf", false, true, ".orf")},
        {".rw2", makeClassification(FileType::Image_Raw, "image/x-panasonic-rw2", false, true, ".rw2")},

        {".zip", makeClassification(FileType::Archive_ZIP, "application/zip", true, false, ".zip")},
        {".tar", makeClassification(FileType::Archive_TAR, "application/x-tar", true, false, ".tar")},
        {".7z", makeClassification(FileType::Archive_7Z, "application/x-7z-compressed", true, false, ".7z")},
        {".gz", makeClassification(FileType::Archive_ZIP, "application/gzip", true, false, ".gz")},
        {".xz", makeClassification(FileType::Archive_7Z, "application/x-xz", true, true, ".xz")},
        {".rar", makeClassification(FileType::Archive_ZIP, "application/vnd.rar", true, false, ".rar")},
        {".zst", makeClassification(FileType::Archive_7Z, "application/zstd", true, false, ".zst")},

        {".db", makeClassification(FileType::Binary_Database, "application/octet-stream", false, true, ".db")},
        {".mdb", makeClassification(FileType::Binary_Database, "application/octet-stream", false, true, ".mdb")},
        {".bin", makeClassification(FileType::Binary_Chunked, "application/octet-stream", false, true, ".bin")},
        {".dat", makeClassification(FileType::Binary_Chunked, "application/octet-stream", false, true, ".dat")},
        {".wasm", makeClassification(FileType::Binary_Executable, "application/wasm", false, true, ".wasm")},
        {".exe", makeClassification(FileType::Binary_Executable, "application/vnd.microsoft.portable-executable", false, true, ".exe")}
    };

    const auto it = extensionMap.find(normalized);
    if (it != extensionMap.end()) {
        return it->second;
    }

    return {};
}

FileClassification FileTypeDetector::detectTextContent(const std::vector<uint8_t>& buffer) {
    if (buffer.empty() || !isValidUtf8(buffer)) {
        return {};
    }

    size_t printableCount = 0;
    for (const auto byte : buffer) {
        if (byte == 9 || byte == 10 || byte == 13 || (byte >= 32 && byte <= 126) || byte >= 128) {
            ++printableCount;
        }
    }

    const double printableRatio = static_cast<double>(printableCount) / static_cast<double>(buffer.size());
    if (printableRatio < 0.95) {
        return {};
    }

    const std::string text(buffer.begin(), buffer.end());
    const std::string trimmed = trimLeft(text);

    if (!trimmed.empty() && (trimmed.front() == '{' || trimmed.front() == '[')) {
        return makeClassification(FileType::Text_Structured, "application/json", false, true, "");
    }

    if (!trimmed.empty() && trimmed.front() == '<') {
        return makeClassification(FileType::Text_Structured, "application/xml", false, true, "");
    }

    static const std::regex logPattern(
        R"(^\s*(\d{4}-\d{2}-\d{2}|\d{2}:\d{2}:\d{2}|\[[A-Z]+\]))",
        std::regex::ECMAScript);
    if (std::regex_search(trimmed, logPattern)) {
        return makeClassification(FileType::Text_Log, "text/plain", false, true, "");
    }

    const size_t codeSignals =
        (text.find("#include") != std::string::npos ? 1U : 0U) +
        (text.find("class ") != std::string::npos ? 1U : 0U) +
        (text.find("def ") != std::string::npos ? 1U : 0U) +
        (text.find("function ") != std::string::npos ? 1U : 0U) +
        (text.find(';') != std::string::npos ? 1U : 0U) +
        (text.find('{') != std::string::npos ? 1U : 0U) +
        (text.find('}') != std::string::npos ? 1U : 0U);

    if (codeSignals >= 2) {
        return makeClassification(FileType::Text_SourceCode, "text/plain", false, true, "");
    }

    return makeClassification(FileType::Text_Prose, "text/plain", false, true, "");
}

void FileTypeDetector::registerCommonTypes() {
    magicEntries_ = {
        {{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}, 0, FileType::Image_PNG, "image/png", true, true},
        {{0xFF, 0xD8, 0xFF}, 0, FileType::Image_JPEG, "image/jpeg", true, false},
        {{0x49, 0x49, 0x2A, 0x00}, 0, FileType::Image_Raw, "image/tiff", false, true},
        {{0x4D, 0x4D, 0x00, 0x2A}, 0, FileType::Image_Raw, "image/tiff", false, true},
        {{0x47, 0x49, 0x46, 0x38}, 0, FileType::Image_Raw, "image/gif", true, true},
        {{0x42, 0x4D}, 0, FileType::Image_Raw, "image/bmp", false, true},
        {{0x25, 0x50, 0x44, 0x46}, 0, FileType::Binary_Chunked, "application/pdf", true, true},

        {{0x50, 0x4B, 0x03, 0x04}, 0, FileType::Archive_ZIP, "application/zip", true, false},
        {{0x50, 0x4B, 0x05, 0x06}, 0, FileType::Archive_ZIP, "application/zip", true, false},
        {{0x1F, 0x8B}, 0, FileType::Archive_ZIP, "application/gzip", true, false},
        {{0x28, 0xB5, 0x2F, 0xFD}, 0, FileType::Archive_7Z, "application/zstd", true, false},
        {{0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00}, 0, FileType::Archive_7Z, "application/x-xz", true, true},
        {{0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C}, 0, FileType::Archive_7Z, "application/x-7z-compressed", true, false},

        {{0x7F, 0x45, 0x4C, 0x46}, 0, FileType::Binary_Executable, "application/x-elf", false, true},
        {{0x4D, 0x5A}, 0, FileType::Binary_Executable, "application/vnd.microsoft.portable-executable", false, true},
        {{0x53, 0x51, 0x4C, 0x69, 0x74, 0x65, 0x20, 0x66, 0x6F, 0x72, 0x6D, 0x61, 0x74, 0x20, 0x33, 0x00}, 0, FileType::Binary_Database, "application/vnd.sqlite3", false, true},

        {{0x1A, 0x45, 0xDF, 0xA3}, 0, FileType::Video_MKV, "video/x-matroska", true, true},
        {{0x49, 0x44, 0x33}, 0, FileType::Audio_MP3, "audio/mpeg", true, false},
        {{0x66, 0x4C, 0x61, 0x43}, 0, FileType::Audio_FLAC, "audio/flac", true, false}
    };

    std::sort(
        magicEntries_.begin(),
        magicEntries_.end(),
        [](const MagicEntry& lhs, const MagicEntry& rhs) {
            return lhs.bytes.size() > rhs.bytes.size();
        });
}

} // namespace mosqueeze
