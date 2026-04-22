#include <mosqueeze/viz/ComparisonEngine.hpp>
#include <mosqueeze/viz/DashboardGenerator.hpp>
#include <mosqueeze/viz/DataLoader.hpp>
#include <mosqueeze/viz/MarkdownExporter.hpp>

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

nlohmann::json computeSummary(const std::vector<mosqueeze::viz::BenchmarkRow>& rows) {
    struct Agg {
        double ratio = 0.0;
        double encode = 0.0;
        double decode = 0.0;
        size_t count = 0;
    };
    std::map<std::string, Agg> byAlgo;
    for (const auto& row : rows) {
        auto& agg = byAlgo[row.algorithm];
        agg.ratio += row.ratio();
        agg.encode += static_cast<double>(row.encodeTime.count());
        agg.decode += static_cast<double>(row.decodeTime.count());
        ++agg.count;
    }

    nlohmann::json out;
    out["rows"] = rows.size();
    out["algorithms"] = nlohmann::json::array();
    for (const auto& [algo, agg] : byAlgo) {
        const double denom = agg.count == 0 ? 1.0 : static_cast<double>(agg.count);
        out["algorithms"].push_back({
            {"algorithm", algo},
            {"rows", agg.count},
            {"avgRatio", agg.ratio / denom},
            {"avgEncodeMs", agg.encode / denom},
            {"avgDecodeMs", agg.decode / denom}
        });
    }
    return out;
}

} // namespace

int main(int argc, char* argv[]) {
    CLI::App app{"MoSqueeze Benchmark Visualization Tool", "mosqueeze-viz"};

    std::filesystem::path inputFile{"benchmarks/results/results.json"};
    std::filesystem::path outputFile;
    std::string format = "html";
    std::filesystem::path compareFile;
    bool diffOnly = false;
    double threshold = 5.0;
    size_t topN = 0;
    std::string sortBy = "ratio";
    bool noCharts = false;
    bool verbose = false;

    app.add_option("-i,--input", inputFile, "Input results file (JSON, CSV, SQLite)");
    app.add_option("-o,--output", outputFile, "Output file path");
    app.add_option("-f,--format", format, "Output format: html, markdown, json-summary")
        ->check(CLI::IsMember({"html", "markdown", "json-summary"}));
    app.add_option("--compare", compareFile, "Compare with baseline results (JSON/CSV/SQLite)");
    app.add_flag("--diff-only", diffOnly, "Show only significant differences");
    app.add_option("--threshold", threshold, "Minimum % difference to highlight")->check(CLI::NonNegativeNumber);
    app.add_option("--top", topN, "Show top N rows (0 = all)")->check(CLI::NonNegativeNumber);
    app.add_option("--sort-by", sortBy, "Sort by: ratio, encode, decode, memory")
        ->check(CLI::IsMember({"ratio", "encode", "decode", "memory"}));
    app.add_flag("--no-charts", noCharts, "Disable embedded charts in HTML output");
    app.add_flag("-v,--verbose", verbose, "Verbose output");

    CLI11_PARSE(app, argc, argv);

    if (outputFile.empty()) {
        if (format == "markdown") {
            outputFile = "results.md";
        } else if (format == "html") {
            outputFile = "dashboard.html";
        }
    }

    try {
        auto current = mosqueeze::viz::DataLoader::load(inputFile);
        if (verbose) {
            std::cout << "Loaded " << current.size() << " rows from " << inputFile.string() << "\n";
        }

        if (!compareFile.empty()) {
            auto baseline = mosqueeze::viz::DataLoader::load(compareFile);
            auto comparisons = mosqueeze::viz::ComparisonEngine::compare(current, baseline, threshold);
            if (diffOnly) {
                comparisons = mosqueeze::viz::ComparisonEngine::filterSignificant(comparisons, threshold);
            }

            if (format == "html") {
                if (outputFile.empty()) {
                    outputFile = "comparison.html";
                }
                mosqueeze::viz::DashboardOptions opts{};
                opts.noCharts = true;
                opts.top = topN;
                opts.sortBy = sortBy;
                opts.thresholdPct = threshold;
                mosqueeze::viz::DashboardGenerator::generateComparison(comparisons, outputFile, opts);
            } else if (format == "markdown") {
                const std::string markdown = mosqueeze::viz::MarkdownExporter::exportComparison(comparisons, threshold);
                if (outputFile.empty()) {
                    std::cout << markdown;
                } else {
                    std::ofstream out(outputFile, std::ios::binary);
                    out << markdown;
                }
            } else {
                nlohmann::json summary;
                summary["rows"] = comparisons.size();
                summary["regressions"] = mosqueeze::viz::ComparisonEngine::findRegressions(comparisons).size();
                summary["improvements"] = mosqueeze::viz::ComparisonEngine::findImprovements(comparisons).size();
                std::cout << summary.dump(2) << "\n";
            }
        } else {
            if (format == "html") {
                mosqueeze::viz::DashboardOptions opts{};
                opts.noCharts = noCharts;
                opts.top = topN;
                opts.sortBy = sortBy;
                opts.thresholdPct = threshold;
                mosqueeze::viz::DashboardGenerator::generate(current, outputFile, opts);
            } else if (format == "markdown") {
                const std::string markdown = mosqueeze::viz::MarkdownExporter::exportSummary(current);
                if (outputFile.empty()) {
                    std::cout << markdown;
                } else {
                    std::ofstream out(outputFile, std::ios::binary);
                    out << markdown;
                }
            } else {
                std::cout << computeSummary(current).dump(2) << "\n";
            }
        }

        if (verbose && !outputFile.empty() && format != "json-summary") {
            std::cout << "Wrote output to " << outputFile.string() << "\n";
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
