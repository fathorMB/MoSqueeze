#include <mosqueeze/viz/DashboardGenerator.hpp>

#include <mosqueeze/viz/Charts.hpp>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mosqueeze::viz {
namespace {

std::string htmlEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (char ch : in) {
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

template<typename T>
void maybeResize(std::vector<T>& rows, size_t top) {
    if (top > 0 && rows.size() > top) {
        rows.resize(top);
    }
}

} // namespace

void DashboardGenerator::generate(
    const std::vector<BenchmarkRow>& results,
    const std::filesystem::path& outputPath,
    const DashboardOptions& options) {
    std::vector<BenchmarkRow> rows = results;
    if (options.sortBy == "encode") {
        std::sort(rows.begin(), rows.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.encodeTime < rhs.encodeTime;
        });
    } else if (options.sortBy == "decode") {
        std::sort(rows.begin(), rows.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.decodeTime < rhs.decodeTime;
        });
    } else if (options.sortBy == "memory") {
        std::sort(rows.begin(), rows.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.peakMemoryBytes < rhs.peakMemoryBytes;
        });
    } else {
        std::sort(rows.begin(), rows.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.ratio() > rhs.ratio();
        });
    }
    maybeResize(rows, options.top);

    std::set<std::string> algorithms;
    for (const auto& row : rows) {
        algorithms.insert(row.algorithm);
    }

    std::ostringstream bodyRows;
    for (const auto& row : rows) {
        bodyRows << "<tr>"
                 << "<td>" << htmlEscape(row.file.filename().string()) << "</td>"
                 << "<td>" << htmlEscape(row.algorithm) << "</td>"
                 << "<td>" << row.level << "</td>"
                 << "<td>" << std::fixed << std::setprecision(3) << row.ratio() << "x</td>"
                 << "<td>" << row.encodeTime.count() << "ms</td>"
                 << "<td>" << row.decodeTime.count() << "ms</td>"
                 << "<td>" << row.peakMemoryBytes << "</td>"
                 << "</tr>\n";
    }

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open dashboard output: " + outputPath.string());
    }

    out << "<!doctype html>\n"
        << "<html lang=\"en\"><head><meta charset=\"utf-8\">"
        << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        << "<title>MoSqueeze Benchmark Results</title>";
    if (!options.noCharts) {
        out << "<script src=\"https://cdn.plot.ly/plotly-2.27.0.min.js\"></script>";
    }
    out << "<style>"
        << "body{font-family:Segoe UI,Tahoma,sans-serif;margin:24px;background:#f6f8fb;color:#1b2430;}"
        << ".card{background:#fff;border:1px solid #d9e2ec;border-radius:8px;padding:14px;margin:12px 0;}"
        << "table{border-collapse:collapse;width:100%;background:#fff;}"
        << "th,td{border:1px solid #d9e2ec;padding:8px;text-align:left;font-size:14px;}"
        << "th{background:#334e68;color:#fff;}"
        << "tr:nth-child(even){background:#f8fbff;}"
        << "</style></head><body>\n"
        << "<h1>MoSqueeze Benchmark Dashboard</h1>\n"
        << "<div class=\"card\"><strong>Rows:</strong> " << rows.size()
        << " &nbsp; <strong>Algorithms:</strong> " << algorithms.size() << "</div>\n";

    if (!options.noCharts) {
        out << "<div class=\"card\"><div id=\"scatter\" style=\"height:420px\"></div></div>\n";
    }

    out << "<table><thead><tr><th>File</th><th>Algorithm</th><th>Level</th><th>Ratio</th><th>Encode</th><th>Decode</th><th>Memory</th></tr></thead><tbody>\n"
        << bodyRows.str()
        << "</tbody></table>\n";

    if (!options.noCharts) {
        out << "<script>\n"
            << "const traces=" << buildScatterPlotJs(rows) << ";\n"
            << "Plotly.newPlot('scatter', traces, {title:'Compression Ratio vs Encode Time',xaxis:{title:'Encode Time (ms)'},yaxis:{title:'Ratio'}});\n"
            << "</script>\n";
    }

    out << "</body></html>\n";
}

void DashboardGenerator::generateComparison(
    const std::vector<ComparisonResult>& comparisons,
    const std::filesystem::path& outputPath,
    const DashboardOptions& options) {
    std::vector<ComparisonResult> rows = comparisons;
    if (options.sortBy == "encode") {
        std::sort(rows.begin(), rows.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.encodeDeltaPct > rhs.encodeDeltaPct;
        });
    } else {
        std::sort(rows.begin(), rows.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.ratioDeltaPct < rhs.ratioDeltaPct;
        });
    }
    maybeResize(rows, options.top);

    std::ostringstream bodyRows;
    for (const auto& row : rows) {
        std::string cls;
        if (row.isRegression) {
            cls = " class=\"regression\"";
        } else if (row.isImprovement) {
            cls = " class=\"improvement\"";
        }

        bodyRows << "<tr" << cls << ">"
                 << "<td>" << htmlEscape(row.file.filename().string()) << "</td>"
                 << "<td>" << htmlEscape(row.algorithm) << "</td>"
                 << "<td>" << row.level << "</td>"
                 << "<td>" << std::fixed << std::setprecision(2) << row.ratioDeltaPct << "%</td>"
                 << "<td>" << std::fixed << std::setprecision(2) << row.encodeDeltaPct << "%</td>"
                 << "<td>" << std::fixed << std::setprecision(2) << row.decodeDeltaPct << "%</td>"
                 << "<td>" << (row.isRegression ? "regression" : (row.isImprovement ? "improvement" : "neutral")) << "</td>"
                 << "</tr>\n";
    }

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open comparison dashboard output: " + outputPath.string());
    }

    out << "<!doctype html>\n"
        << "<html lang=\"en\"><head><meta charset=\"utf-8\">"
        << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        << "<title>MoSqueeze Benchmark Comparison</title>"
        << "<style>"
        << "body{font-family:Segoe UI,Tahoma,sans-serif;margin:24px;background:#f6f8fb;color:#1b2430;}"
        << "table{border-collapse:collapse;width:100%;background:#fff;}"
        << "th,td{border:1px solid #d9e2ec;padding:8px;text-align:left;font-size:14px;}"
        << "th{background:#334e68;color:#fff;}"
        << ".regression{background:#fed7d7;}"
        << ".improvement{background:#c6f6d5;}"
        << "</style></head><body>\n"
        << "<h1>MoSqueeze Benchmark Comparison</h1>\n"
        << "<p>Threshold: " << std::fixed << std::setprecision(2) << options.thresholdPct << "%</p>\n"
        << "<table><thead><tr><th>File</th><th>Algorithm</th><th>Level</th><th>Ratio Δ%</th><th>Encode Δ%</th><th>Decode Δ%</th><th>Status</th></tr></thead><tbody>\n"
        << bodyRows.str()
        << "</tbody></table>\n"
        << "</body></html>\n";
}

} // namespace mosqueeze::viz
