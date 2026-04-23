#pragma once

#include <CLI/CLI.hpp>

#include <memory>
#include <string>

namespace mosqueeze {
class ICompressionEngine;
class IPreprocessor;
}

namespace mosqueeze::cli {

struct CompressOptions {
    std::string inputFile;
    std::string outputFile;
    std::string algorithm = "zstd";
    int level = -1;
    std::string preprocess = "none";
    bool jsonOutput = false;
};

void addCompressCommand(CLI::App& app);

std::unique_ptr<ICompressionEngine> createEngine(const std::string& algorithm);
std::unique_ptr<IPreprocessor> createPreprocessor(const std::string& preprocess);
int defaultLevelFor(const std::string& algorithm);
int runCompress(const CompressOptions& opts);

} // namespace mosqueeze::cli
