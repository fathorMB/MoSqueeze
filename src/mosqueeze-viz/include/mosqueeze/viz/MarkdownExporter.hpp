#pragma once

#include <mosqueeze/viz/ComparisonEngine.hpp>
#include <mosqueeze/viz/Types.hpp>

#include <string>
#include <vector>

namespace mosqueeze::viz {

class MarkdownExporter {
public:
    static std::string exportSummary(
        const std::vector<BenchmarkRow>& results,
        const std::string& title = "Benchmark Results");

    static std::string exportComparison(
        const std::vector<ComparisonResult>& comparisons,
        double thresholdPct = 5.0);
};

} // namespace mosqueeze::viz
