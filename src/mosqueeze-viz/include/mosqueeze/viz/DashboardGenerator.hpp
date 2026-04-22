#pragma once

#include <mosqueeze/viz/ComparisonEngine.hpp>
#include <mosqueeze/viz/Types.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace mosqueeze::viz {

struct DashboardOptions {
    bool noCharts = false;
    size_t top = 0;
    std::string sortBy = "ratio";
    double thresholdPct = 5.0;
};

class DashboardGenerator {
public:
    static void generate(
        const std::vector<BenchmarkRow>& results,
        const std::filesystem::path& outputPath,
        const DashboardOptions& options = {});

    static void generateComparison(
        const std::vector<ComparisonResult>& comparisons,
        const std::filesystem::path& outputPath,
        const DashboardOptions& options = {});
};

} // namespace mosqueeze::viz
