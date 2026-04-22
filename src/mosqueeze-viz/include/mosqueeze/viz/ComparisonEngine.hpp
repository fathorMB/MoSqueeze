#pragma once

#include <mosqueeze/viz/Types.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace mosqueeze::viz {

struct ComparisonResult {
    std::string algorithm;
    int level = 0;
    std::filesystem::path file;

    double ratioBefore = 0.0;
    double ratioAfter = 0.0;
    double ratioDelta = 0.0;
    double ratioDeltaPct = 0.0;

    double encodeBefore = 0.0;
    double encodeAfter = 0.0;
    double encodeDelta = 0.0;
    double encodeDeltaPct = 0.0;

    double decodeBefore = 0.0;
    double decodeAfter = 0.0;
    double decodeDelta = 0.0;
    double decodeDeltaPct = 0.0;

    bool isRegression = false;
    bool isImprovement = false;
};

class ComparisonEngine {
public:
    static std::vector<ComparisonResult> compare(
        const std::vector<BenchmarkRow>& current,
        const std::vector<BenchmarkRow>& baseline,
        double thresholdPct = 5.0);

    static std::vector<ComparisonResult> filterSignificant(
        const std::vector<ComparisonResult>& comparisons,
        double thresholdPct = 5.0);

    static std::vector<std::string> findRegressions(const std::vector<ComparisonResult>& results);
    static std::vector<std::string> findImprovements(const std::vector<ComparisonResult>& results);
};

} // namespace mosqueeze::viz
