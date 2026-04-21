#pragma once

#include <mosqueeze/Types.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace mosqueeze::bench {

struct CorpusFile {
    std::filesystem::path path;
    FileType type = FileType::Unknown;
    std::string description;
    size_t sizeBytes = 0;
};

class CorpusManager {
public:
    CorpusManager();
    explicit CorpusManager(std::filesystem::path corpusDir);

    std::filesystem::path corpusDirectory() const;
    std::vector<CorpusFile> listFiles(FileType filter = FileType::Unknown) const;

    void addFile(const std::filesystem::path& file, FileType type, const std::string& description = "");
    void downloadStandardCorpus();

    size_t totalFiles() const;
    size_t totalSize() const;

private:
    std::filesystem::path corpusDir_;
};

} // namespace mosqueeze::bench
