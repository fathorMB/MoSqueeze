#include <mosqueeze/bench/CorpusManager.hpp>

#include <mosqueeze/FileTypeDetector.hpp>

#include <fstream>
#include <stdexcept>

namespace mosqueeze::bench {

CorpusManager::CorpusManager()
    : CorpusManager(std::filesystem::path("benchmarks") / "corpus") {}

CorpusManager::CorpusManager(std::filesystem::path corpusDir)
    : corpusDir_(std::move(corpusDir)) {
    std::filesystem::create_directories(corpusDir_);
}

std::filesystem::path CorpusManager::corpusDirectory() const {
    return corpusDir_;
}

std::vector<CorpusFile> CorpusManager::listFiles(FileType filter) const {
    std::vector<CorpusFile> files;
    if (!std::filesystem::exists(corpusDir_)) {
        return files;
    }

    FileTypeDetector detector;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(corpusDir_)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const auto classification = detector.detect(entry.path());
        if (filter != FileType::Unknown && classification.type != filter) {
            continue;
        }

        CorpusFile file{};
        file.path = entry.path();
        file.type = classification.type;
        file.description = entry.path().filename().string();
        file.sizeBytes = static_cast<size_t>(entry.file_size());
        files.push_back(std::move(file));
    }

    return files;
}

void CorpusManager::addFile(const std::filesystem::path& file, FileType type, const std::string& description) {
    if (!std::filesystem::exists(file) || !std::filesystem::is_regular_file(file)) {
        throw std::runtime_error("CorpusManager::addFile input does not exist: " + file.string());
    }

    std::filesystem::create_directories(corpusDir_);
    const auto destination = corpusDir_ / file.filename();
    std::filesystem::copy_file(file, destination, std::filesystem::copy_options::overwrite_existing);

    (void)type;
    (void)description;
}

void CorpusManager::downloadStandardCorpus() {
    std::filesystem::create_directories(corpusDir_);
    const auto readme = corpusDir_ / "README.md";
    if (!std::filesystem::exists(readme)) {
        std::ofstream out(readme, std::ios::binary);
        out << "# Benchmark Corpus\n\n";
        out << "Add representative files here for benchmark runs.\n";
    }
}

size_t CorpusManager::totalFiles() const {
    return listFiles().size();
}

size_t CorpusManager::totalSize() const {
    size_t total = 0;
    for (const auto& file : listFiles()) {
        total += file.sizeBytes;
    }
    return total;
}

} // namespace mosqueeze::bench
