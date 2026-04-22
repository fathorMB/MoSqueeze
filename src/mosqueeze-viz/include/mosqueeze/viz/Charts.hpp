#pragma once

#include <mosqueeze/viz/Types.hpp>

#include <string>
#include <vector>

namespace mosqueeze::viz {

std::string buildScatterPlotJs(const std::vector<BenchmarkRow>& results);

} // namespace mosqueeze::viz
