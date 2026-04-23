#pragma once

#include <CLI/CLI.hpp>

#include <string>

namespace mosqueeze::cli {

struct DecompressOptions {
    std::string inputFile;
    std::string outputFile;
    bool jsonOutput = false;
};

void addDecompressCommand(CLI::App& app);
int runDecompress(const DecompressOptions& opts);

} // namespace mosqueeze::cli
