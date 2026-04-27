#pragma once

#include "../Terminal.hpp"

#include <CLI/CLI.hpp>

namespace mosqueeze::cli {

void addPredictCommand(CLI::App& app, const Terminal& term);

} // namespace mosqueeze::cli