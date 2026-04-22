#include <mosqueeze/viz/Charts.hpp>

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

namespace mosqueeze::viz {

std::string buildScatterPlotJs(const std::vector<BenchmarkRow>& results) {
    std::map<std::string, nlohmann::json> tracesByAlgorithm;
    for (const auto& row : results) {
        auto& trace = tracesByAlgorithm[row.algorithm];
        trace["name"] = row.algorithm;
        trace["type"] = "scatter";
        trace["mode"] = "markers";
        trace["x"].push_back(row.encodeTime.count());
        trace["y"].push_back(row.ratio());
        trace["text"].push_back(row.file.filename().string() + " L" + std::to_string(row.level));
    }

    nlohmann::json traces = nlohmann::json::array();
    for (const auto& [_, trace] : tracesByAlgorithm) {
        traces.push_back(trace);
    }

    return traces.dump();
}

} // namespace mosqueeze::viz
