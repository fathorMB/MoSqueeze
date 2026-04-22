#include <mosqueeze/bench/Formatters.hpp>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace mosqueeze::bench::Formatters {
namespace {

std::string formatDuration(std::chrono::milliseconds duration) {
    std::ostringstream out;
    out << duration.count() << "ms";
    return out.str();
}

std::string formatBytes(size_t bytes) {
    static const char* suffixes[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    size_t suffix = 0;
    while (value >= 1024.0 && suffix < (sizeof(suffixes) / sizeof(suffixes[0])) - 1) {
        value /= 1024.0;
        ++suffix;
    }

    std::ostringstream out;
    out << std::fixed << std::setprecision(suffix == 0 ? 0 : 2) << value << suffixes[suffix];
    return out.str();
}

std::string formatRatio(double ratio) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << ratio << "x";
    return out.str();
}

std::string statsKey(const BenchmarkResult& result) {
    return result.algorithm + "/" + std::to_string(result.level);
}

} // namespace

std::string formatVerbose(const BenchmarkResult& result, const BenchmarkStats* stats) {
    std::ostringstream out;
    out << result.file.filename().string() << " | "
        << result.algorithm << "/" << result.level << " | ratio "
        << std::fixed << std::setprecision(3) << result.ratio() << "x | encode "
        << formatDuration(result.encodeTime) << " | decode " << formatDuration(result.decodeTime)
        << " | memory " << formatBytes(result.peakMemoryBytes);

    if (result.preprocess.applied) {
        out << " | preprocess " << result.preprocess.type
            << " (" << std::fixed << std::setprecision(2) << result.preprocess.preprocessingTimeMs << "ms)";
    }

    if (stats != nullptr) {
        out << " | mean ratio " << std::fixed << std::setprecision(3) << stats->meanRatio
            << " +/- " << stats->stdDevRatio;
    }
    return out.str();
}

std::string formatSummaryTable(
    const std::vector<BenchmarkResult>& results,
    const std::unordered_map<std::string, BenchmarkStats>* stats) {
    std::vector<BenchmarkResult> sorted = results;
    std::sort(sorted.begin(), sorted.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.algorithm != rhs.algorithm) {
            return lhs.algorithm < rhs.algorithm;
        }
        if (lhs.level != rhs.level) {
            return lhs.level < rhs.level;
        }
        return lhs.ratio() > rhs.ratio();
    });

    std::ostringstream out;
    out << "Algorithm      Level  Ratio    Encode   Decode   Memory     Preprocess\n";
    out << "-----------------------------------------------------------------------\n";
    for (const auto& result : sorted) {
        out << std::left << std::setw(14) << result.algorithm
            << std::setw(7) << result.level
            << std::setw(9) << formatRatio(result.ratio())
            << std::setw(9) << formatDuration(result.encodeTime)
            << std::setw(9) << formatDuration(result.decodeTime)
            << std::setw(11) << formatBytes(result.peakMemoryBytes)
            << (result.preprocess.applied ? result.preprocess.type : "none");

        if (stats != nullptr) {
            auto it = stats->find(statsKey(result));
            if (it != stats->end()) {
                out << "  (n=" << it->second.iterations << ")";
            }
        }

        out << '\n';
    }
    return out.str();
}

std::string exportMarkdown(
    const std::vector<BenchmarkResult>& results,
    const std::unordered_map<std::string, BenchmarkStats>* stats) {
    std::map<std::filesystem::path, std::vector<BenchmarkResult>> byFile;
    for (const auto& result : results) {
        byFile[result.file].push_back(result);
    }

    std::ostringstream out;
    out << "# Benchmark Results\n\n";
    for (const auto& [file, rows] : byFile) {
        out << "## " << file.filename().string() << "\n\n";
        out << "| Algorithm | Level | Ratio | Encode | Decode | Memory | Preprocess | Preprocess Time | Stats |\n";
        out << "|---|---:|---:|---:|---:|---:|---|---:|---|\n";

        std::vector<BenchmarkResult> sorted = rows;
        std::sort(sorted.begin(), sorted.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.ratio() > rhs.ratio();
        });

        for (const auto& result : sorted) {
            out << "| " << result.algorithm
                << " | " << result.level
                << " | " << std::fixed << std::setprecision(3) << result.ratio() << "x"
                << " | " << result.encodeTime.count() << "ms"
                << " | " << result.decodeTime.count() << "ms"
                << " | " << formatBytes(result.peakMemoryBytes)
                << " | " << (result.preprocess.applied ? result.preprocess.type : "none")
                << " | " << std::fixed << std::setprecision(2) << result.preprocess.preprocessingTimeMs << "ms"
                << " | ";
            if (stats != nullptr) {
                auto it = stats->find(statsKey(result));
                if (it != stats->end()) {
                    out << "n=" << it->second.iterations << ", mean=" << std::fixed << std::setprecision(3)
                        << it->second.meanRatio << "x";
                }
            }
            out << " |\n";
        }
        out << '\n';
    }
    return out.str();
}

std::string exportHtml(
    const std::vector<BenchmarkResult>& results,
    const std::unordered_map<std::string, BenchmarkStats>* stats) {
    std::ostringstream rows;
    std::vector<BenchmarkResult> sorted = results;
    std::sort(sorted.begin(), sorted.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.file != rhs.file) {
            return lhs.file < rhs.file;
        }
        if (lhs.algorithm != rhs.algorithm) {
            return lhs.algorithm < rhs.algorithm;
        }
        return lhs.level < rhs.level;
    });

    for (const auto& result : sorted) {
        rows << "<tr>"
             << "<td>" << result.file.filename().string() << "</td>"
             << "<td>" << result.algorithm << "</td>"
             << "<td>" << result.level << "</td>"
             << "<td>" << std::fixed << std::setprecision(3) << result.ratio() << "x</td>"
             << "<td>" << result.encodeTime.count() << " ms</td>"
             << "<td>" << result.decodeTime.count() << " ms</td>"
             << "<td>" << formatBytes(result.peakMemoryBytes) << "</td>"
             << "<td>" << (result.preprocess.applied ? result.preprocess.type : "none") << "</td>"
             << "<td>" << std::fixed << std::setprecision(2) << result.preprocess.preprocessingTimeMs << " ms</td>";

        std::string statsCell;
        if (stats != nullptr) {
            auto it = stats->find(statsKey(result));
            if (it != stats->end()) {
                std::ostringstream cell;
                cell << "n=" << it->second.iterations << ", mean ratio "
                     << std::fixed << std::setprecision(3) << it->second.meanRatio << "x";
                statsCell = cell.str();
            }
        }
        rows << "<td>" << statsCell << "</td>";
        rows << "</tr>\n";
    }

    std::ostringstream out;
    out << "<!doctype html>\n"
        << "<html lang=\"en\">\n"
        << "<head>\n"
        << "<meta charset=\"utf-8\">\n"
        << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        << "<title>MoSqueeze Benchmark Results</title>\n"
        << "<script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>\n"
        << "<style>\n"
        << "body{font-family:Segoe UI,Tahoma,sans-serif;margin:24px;background:#f6f8fb;color:#1b2430;}\n"
        << "h1{margin-bottom:12px;}\n"
        << "table{border-collapse:collapse;width:100%;background:white;}\n"
        << "th,td{border:1px solid #d9e2ec;padding:8px;text-align:left;font-size:14px;}\n"
        << "th{background:#334e68;color:white;}\n"
        << "tr:nth-child(even){background:#f8fbff;}\n"
        << ".card{background:white;padding:16px;border:1px solid #d9e2ec;margin-bottom:18px;}\n"
        << "</style>\n"
        << "</head>\n"
        << "<body>\n"
        << "<h1>MoSqueeze Benchmark Results</h1>\n"
        << "<div class=\"card\"><canvas id=\"ratioChart\"></canvas></div>\n"
        << "<table>\n"
        << "<thead><tr><th>File</th><th>Algorithm</th><th>Level</th><th>Ratio</th><th>Encode</th><th>Decode</th><th>Memory</th><th>Preprocess</th><th>Preprocess Time</th><th>Stats</th></tr></thead>\n"
        << "<tbody>\n"
        << rows.str()
        << "</tbody></table>\n"
        << "<script>\n"
        << "const labels=[";

    for (size_t i = 0; i < sorted.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << "\"" << sorted[i].algorithm << "/" << sorted[i].level << "\"";
    }
    out << "];\nconst data=[";
    for (size_t i = 0; i < sorted.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << std::fixed << std::setprecision(3) << sorted[i].ratio();
    }
    out << "];\n"
        << "new Chart(document.getElementById('ratioChart'),{type:'bar',data:{labels,datasets:[{label:'Compression ratio',data,backgroundColor:'#627d98'}]}});\n"
        << "</script>\n"
        << "</body>\n"
        << "</html>\n";
    return out.str();
}

} // namespace mosqueeze::bench::Formatters
