#include <mosqueeze/viz/ComparisonEngine.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace mosqueeze::viz {
namespace {

struct AggRow {
    std::filesystem::path file;
    std::string algorithm;
    int level = 0;
    double ratio = 0.0;
    double encode = 0.0;
    double decode = 0.0;
    size_t count = 0;
};

std::string keyFor(const BenchmarkRow& row) {
    return row.file.string() + "|" + row.algorithm + "|" + std::to_string(row.level);
}

std::unordered_map<std::string, AggRow> aggregateRows(const std::vector<BenchmarkRow>& rows) {
    std::unordered_map<std::string, AggRow> map;
    for (const auto& row : rows) {
        const std::string key = keyFor(row);
        auto& agg = map[key];
        agg.file = row.file;
        agg.algorithm = row.algorithm;
        agg.level = row.level;
        agg.ratio += row.ratio();
        agg.encode += static_cast<double>(row.encodeTime.count());
        agg.decode += static_cast<double>(row.decodeTime.count());
        ++agg.count;
    }

    for (auto& [_, agg] : map) {
        if (agg.count > 0) {
            agg.ratio /= static_cast<double>(agg.count);
            agg.encode /= static_cast<double>(agg.count);
            agg.decode /= static_cast<double>(agg.count);
        }
    }

    return map;
}

double pctDelta(double before, double after) {
    if (std::abs(before) < 1e-9) {
        return 0.0;
    }
    return ((after - before) / before) * 100.0;
}

std::string cmpKey(const ComparisonResult& row) {
    return row.file.string() + "|" + row.algorithm + "|" + std::to_string(row.level);
}

} // namespace

std::vector<ComparisonResult> ComparisonEngine::compare(
    const std::vector<BenchmarkRow>& current,
    const std::vector<BenchmarkRow>& baseline,
    double thresholdPct) {
    const auto currentAgg = aggregateRows(current);
    const auto baselineAgg = aggregateRows(baseline);

    std::vector<ComparisonResult> out;
    out.reserve(currentAgg.size());

    for (const auto& [key, curr] : currentAgg) {
        auto it = baselineAgg.find(key);
        if (it == baselineAgg.end()) {
            continue;
        }
        const auto& base = it->second;

        ComparisonResult row{};
        row.file = curr.file;
        row.algorithm = curr.algorithm;
        row.level = curr.level;

        row.ratioBefore = base.ratio;
        row.ratioAfter = curr.ratio;
        row.ratioDelta = row.ratioAfter - row.ratioBefore;
        row.ratioDeltaPct = pctDelta(row.ratioBefore, row.ratioAfter);

        row.encodeBefore = base.encode;
        row.encodeAfter = curr.encode;
        row.encodeDelta = row.encodeAfter - row.encodeBefore;
        row.encodeDeltaPct = pctDelta(row.encodeBefore, row.encodeAfter);

        row.decodeBefore = base.decode;
        row.decodeAfter = curr.decode;
        row.decodeDelta = row.decodeAfter - row.decodeBefore;
        row.decodeDeltaPct = pctDelta(row.decodeBefore, row.decodeAfter);

        const bool ratioWorse = row.ratioDeltaPct <= -thresholdPct;
        const bool ratioBetter = row.ratioDeltaPct >= thresholdPct;
        const bool encodeWorse = row.encodeDeltaPct >= thresholdPct;
        const bool encodeBetter = row.encodeDeltaPct <= -thresholdPct;
        const bool decodeWorse = row.decodeDeltaPct >= thresholdPct;
        const bool decodeBetter = row.decodeDeltaPct <= -thresholdPct;

        row.isRegression = ratioWorse || encodeWorse || decodeWorse;
        row.isImprovement = ratioBetter || encodeBetter || decodeBetter;
        out.push_back(std::move(row));
    }

    std::sort(out.begin(), out.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.file != rhs.file) {
            return lhs.file < rhs.file;
        }
        if (lhs.algorithm != rhs.algorithm) {
            return lhs.algorithm < rhs.algorithm;
        }
        return lhs.level < rhs.level;
    });
    return out;
}

std::vector<ComparisonResult> ComparisonEngine::filterSignificant(
    const std::vector<ComparisonResult>& comparisons,
    double thresholdPct) {
    std::vector<ComparisonResult> out;
    for (const auto& row : comparisons) {
        const bool significant =
            std::abs(row.ratioDeltaPct) >= thresholdPct ||
            std::abs(row.encodeDeltaPct) >= thresholdPct ||
            std::abs(row.decodeDeltaPct) >= thresholdPct;
        if (significant) {
            out.push_back(row);
        }
    }
    return out;
}

std::vector<std::string> ComparisonEngine::findRegressions(const std::vector<ComparisonResult>& results) {
    std::vector<std::string> out;
    out.reserve(results.size());
    for (const auto& row : results) {
        if (row.isRegression) {
            out.push_back(cmpKey(row));
        }
    }
    return out;
}

std::vector<std::string> ComparisonEngine::findImprovements(const std::vector<ComparisonResult>& results) {
    std::vector<std::string> out;
    out.reserve(results.size());
    for (const auto& row : results) {
        if (row.isImprovement) {
            out.push_back(cmpKey(row));
        }
    }
    return out;
}

} // namespace mosqueeze::viz
