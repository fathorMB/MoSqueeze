#include <mosqueeze/viz/MarkdownExporter.hpp>

#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace mosqueeze::viz {
namespace {

std::string ratioFmt(double ratio) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << ratio << "x";
    return out.str();
}

std::string msFmt(double ms) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << ms << "ms";
    return out.str();
}

} // namespace

std::string MarkdownExporter::exportSummary(
    const std::vector<BenchmarkRow>& results,
    const std::string& title) {
    struct Agg {
        double ratio = 0.0;
        double encode = 0.0;
        double decode = 0.0;
        size_t count = 0;
    };

    std::map<std::string, Agg> byAlgo;
    for (const auto& row : results) {
        auto& agg = byAlgo[row.algorithm];
        agg.ratio += row.ratio();
        agg.encode += static_cast<double>(row.encodeTime.count());
        agg.decode += static_cast<double>(row.decodeTime.count());
        ++agg.count;
    }

    std::ostringstream out;
    out << "# " << title << "\n\n";
    out << "## Summary\n\n";
    out << "| Algorithm | Rows | Avg Ratio | Avg Encode | Avg Decode |\n";
    out << "|---|---:|---:|---:|---:|\n";
    for (const auto& [algo, agg] : byAlgo) {
        const double denom = agg.count == 0 ? 1.0 : static_cast<double>(agg.count);
        out << "| " << algo
            << " | " << agg.count
            << " | " << ratioFmt(agg.ratio / denom)
            << " | " << msFmt(agg.encode / denom)
            << " | " << msFmt(agg.decode / denom)
            << " |\n";
    }

    out << "\n## Full Results\n\n";
    out << "| File | Algorithm | Level | Ratio | Encode | Decode | Memory |\n";
    out << "|---|---|---:|---:|---:|---:|---:|\n";
    for (const auto& row : results) {
        out << "| " << row.file.filename().string()
            << " | " << row.algorithm
            << " | " << row.level
            << " | " << ratioFmt(row.ratio())
            << " | " << row.encodeTime.count() << "ms"
            << " | " << row.decodeTime.count() << "ms"
            << " | " << row.peakMemoryBytes
            << " |\n";
    }
    return out.str();
}

std::string MarkdownExporter::exportComparison(
    const std::vector<ComparisonResult>& comparisons,
    double thresholdPct) {
    std::ostringstream out;
    out << "# Benchmark Comparison\n\n";
    out << "Threshold: " << std::fixed << std::setprecision(2) << thresholdPct << "%\n\n";
    out << "| File | Algorithm | Level | Ratio Δ% | Encode Δ% | Decode Δ% | Status |\n";
    out << "|---|---|---:|---:|---:|---:|---|\n";
    for (const auto& row : comparisons) {
        std::string status = "neutral";
        if (row.isRegression) {
            status = "regression";
        } else if (row.isImprovement) {
            status = "improvement";
        }
        out << "| " << row.file.filename().string()
            << " | " << row.algorithm
            << " | " << row.level
            << " | " << std::fixed << std::setprecision(2) << row.ratioDeltaPct
            << " | " << std::fixed << std::setprecision(2) << row.encodeDeltaPct
            << " | " << std::fixed << std::setprecision(2) << row.decodeDeltaPct
            << " | " << status
            << " |\n";
    }
    return out.str();
}

} // namespace mosqueeze::viz
