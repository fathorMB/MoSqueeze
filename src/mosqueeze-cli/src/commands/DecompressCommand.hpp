#pragma once

#include "../Terminal.hpp"

#include <CLI/CLI.hpp>

#include <string>

namespace mosqueeze::cli {

struct DecompressOptions {
    std::string inputFile;
    std::string outputFile;
    bool jsonOutput = false;
};

void addDecompressCommand(CLI::App& app, const Terminal& term);
int runDecompress(const DecompressOptions& opts, const Terminal& term);

} // namespace mosqueeze::cli