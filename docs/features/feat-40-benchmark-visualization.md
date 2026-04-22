# Worker Spec: Benchmark Results Visualization Tool (Issue #40)

## Overview

Create a post-benchmark visualization tool (`mosqueeze-viz`) to display benchmark results in user-friendly formats (HTML dashboard, Markdown reports).

## Background

MoSqueeze benchmark runs produce structured data:
- `benchmarks/results/results.json` — raw JSON results
- `benchmarks/results/results.csv` — tabular CSV
- `benchmarks/results/results.sqlite3` — SQLite database

These formats are not ideal for quick analysis. Users need:
- Visual comparison of algorithms (ratio vs speed)
- Summary tables for reports
- Historical trend analysis
- Regression detection between runs

## Proposed Solution

### New Executable: `mosqueeze-viz`

Location: `src/mosqueeze-viz/`

```
mosqueeze-viz [OPTIONS]

Input:
  -i, --input FILE       Input results file (JSON, CSV, or SQLite)
                         Default: benchmarks/results/results.json

Output:
  -o, --output FILE      Output file path
                         Default: dashboard.html (or results.md for markdown)

Format:
  -f, --format FORMAT    Output format: html, markdown, json-summary
                         Default: html

Comparison:
  --compare FILE         Compare with baseline results (JSON or CSV)
  --diff-only            Show only files with significant differences

Options:
  --threshold PCT        Minimum % difference to highlight (default: 5.0)
  --top N                Show top N results per file (default: all)
  --sort-by COLUMN       Sort by: ratio, encode, decode, memory
  --no-charts            Disable embedded charts (HTML only)
  -v, --verbose          Verbose output
  -h, --help             Show help
```

### Example Usage

```bash
# Generate HTML dashboard from latest results
mosqueeze-viz -i results.json -o dashboard.html

# Compare with previous baseline
mosqueeze-viz -i results.json --compare baseline.json -o comparison.html

# Export summary as markdown
mosqueeze-viz -i results.json --format markdown -o results.md

# Quick JSON summary for scripts
mosqueeze-viz -i results.json --format json-summary
```

---

## Architecture

### Directory Structure

```
src/mosqueeze-viz/
├── CMakeLists.txt
├── src/
│   ├── main.cpp              # CLI entry point
│   ├── DataLoader.cpp        # Load JSON, CSV, SQLite
│   ├── DashboardGenerator.cpp # HTML generation
│   ├── MarkdownExporter.cpp  # Markdown table export
│   ├── ComparisonEngine.cpp   # Diff between runs
│   └── Charts.cpp            # Chart data generation
└── include/mosqueeze/viz/
    ├── DataLoader.hpp
    ├── DashboardGenerator.hpp
    ├── MarkdownExporter.hpp
    ├── ComparisonEngine.hpp
    └── Charts.hpp
```

### Dependencies

- **nlohmann/json** — JSON parsing (already in vcpkg)
- **sqlite3** — SQLite queries (already used in ResultsStore)
- **No external HTML libs** — generate HTML programmatically
- **Optional: Plotly CDN** — embedded interactive charts (with `--no-charts` fallback)

---

## Implementation Details

### Part 1: DataLoader

Load benchmark results from any supported format.

```cpp
namespace mosqueeze::viz {

struct BenchmarkRow {
    std::string algorithm;
    int level;
    std::filesystem::path file;
    int fileType;  // enum FileType
    size_t originalBytes;
    size_t compressedBytes;
    std::chrono::milliseconds encodeTime;
    std::chrono::milliseconds decodeTime;
    size_t peakMemoryBytes;
    
    double ratio() const {
        return static_cast<double>(originalBytes) / compressedBytes;
    }
};

class DataLoader {
public:
    static std::vector<BenchmarkRow> loadJson(const std::filesystem::path& path);
    static std::vector<BenchmarkRow> loadCsv(const std::filesystem::path& path);
    static std::vector<BenchmarkRow> loadSqlite(const std::filesystem::path& path);
    static std::vector<BenchmarkRow> load(const std::filesystem::path& path); // auto-detect
};

} // namespace mosqueeze::viz
```

### Part 2: Comparison Engine

Compare two benchmark result sets and identify regressions/improvements.

```cpp
struct ComparisonResult {
    std::string algorithm;
    int level;
    std::filesystem::path file;
    
    double ratioBefore;
    double ratioAfter;
    double ratioDelta;      // % change
    double ratioDeltaPct;   // relative %
    
    double encodeBefore;
    double encodeAfter;
    double encodeDelta;
    double encodeDeltaPct;
    
    double decodeBefore;
    double decodeAfter;
    double decodeDelta;
    double decodeDeltaPct;
    
    bool isRegression;      // ratio decreased or time increased significantly
    bool isImprovement;     // ratio increased or time decreased significantly
};

class ComparisonEngine {
public:
    static std::vector<ComparisonResult> compare(
        const std::vector<BenchmarkRow>& current,
        const std::vector<BenchmarkRow>& baseline,
        double thresholdPct = 5.0
    );
    
    static std::vector<std::string> findRegressions(const std::vector<ComparisonResult>& results);
    static std::vector<std::string> findImprovements(const std::vector<ComparisonResult>& results);
};
```

### Part 3: Dashboard Generator

Generate HTML dashboard with:
- Summary statistics table
- Per-algorithm breakdown
- Ratio vs. Encode Time scatter plot (Chart.js/Plotly)
- Best algorithm per file type
- Comparison section (if `--compare` provided)

```cpp
class DashboardGenerator {
public:
    static void generate(
        const std::vector<BenchmarkRow>& results,
        const std::filesystem::path& outputPath,
        const DashboardOptions& options = {}
    );
    
    static void generateComparison(
        const std::vector<ComparisonResult>& comparisons,
        const std::filesystem::path& outputPath,
        const DashboardOptions& options = {}
    );
};
```

### HTML Structure

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>MoSqueeze Benchmark Results</title>
    <script src="https://cdn.plot.ly/plotly-2.27.0.min.js"></script>
    <style>
        /* Minimal CSS - no external dependencies */
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; margin: 40px; }
        table { border-collapse: collapse; width: 100%; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #4a5568; color: white; }
        tr:nth-child(even) { background-color: #f7fafc; }
        .regression { background-color: #fed7d7; }
        .improvement { background-color: #c6f6d5; }
        .summary-card { background: #edf2f7; border-radius: 8px; padding: 16px; margin: 16px 0; }
        .chart-container { width: 100%; height: 400px; }
    </style>
</head>
<body>
    <h1>MoSqueeze Benchmark Dashboard</h1>
    
    <!-- Summary Cards -->
    <div class="summary-card">
        <h3>Summary</h3>
        <p><strong>Files:</strong> {{ fileCount }}</p>
        <p><strong>Algorithms:</strong> {{ algorithms }}</p>
        <p><strong>Total Iterations:</strong> {{ totalIterations }}</p>
    </div>
    
    <!-- Ratio vs Speed Scatter Plot -->
    <div id="scatter-plot" class="chart-container"></div>
    
    <!-- Results Table -->
    <h2>Results by Algorithm</h2>
    {{ resultsTable }}
    
    <!-- Comparison Section (if applicable) -->
    {{ comparisonSection }}
    
    <script>
        // Plotly scatter plot data
        const data = {{ chartData }};
        Plotly.newPlot('scatter-plot', data, {
            title: 'Compression Ratio vs Encode Time',
            xaxis: { title: 'Encode Time (ms)' },
            yaxis: { title: 'Compression Ratio' },
            mode: 'markers',
            type: 'scatter'
        });
    </script>
</body>
</html>
```

### Part 4: Markdown Exporter

Generate Markdown tables for documentation.

```cpp
class MarkdownExporter {
public:
    static std::string exportSummary(
        const std::vector<BenchmarkRow>& results,
        const std::string& title = "Benchmark Results"
    );
    
    static std::string exportDetailed(
        const std::vector<BenchmarkRow>& results,
        bool includeStats = true
    );
    
    static std::string exportComparison(
        const std::vector<ComparisonResult>& comparisons,
        double thresholdPct = 5.0
    );
};
```

### Markdown Output Format

```markdown
# Benchmark Results

## Summary

| Algorithm | Files | Avg Ratio | Avg Encode (ms) | Avg Decode (ms) |
|-----------|-------|-----------|-----------------|------------------|
| zstd      | 100   | 2.45x     | 120             | 45               |
| brotli    | 100   | 2.67x     | 890             | 320              |
| xz        | 100   | 2.89x     | 1200            | 450              |
| zpaq      | 100   | 3.12x     | 5400            | 2100             |

## Best by File Type

| File Type | Best Algorithm | Best Ratio | Best Speed |
|-----------|----------------|------------|------------|
| Image     | zstd/22        | 1.85x      | 45ms       |
| Video     | zstd/3         | 1.12x      | 120ms      |
| JSON      | brotli/11      | 4.23x      | 340ms      |

## Full Results

### image.png (1.2 MB)

| Algorithm | Level | Ratio | Encode | Decode | Memory |
|-----------|-------|-------|--------|--------|--------|
| zstd      | 22    | 1.85x | 45ms   | 12ms   | 2.1 MB |
| brotli    | 11    | 1.92x | 340ms  | 89ms   | 3.4 MB |
| xz        | 9     | 1.98x | 520ms  | 78ms   | 4.2 MB |
| zpaq      | 5     | 2.15x | 2300ms | 890ms  | 5.8 MB |
```

---

## CLI Implementation

### main.cpp Structure

```cpp
#include <mosqueeze/viz/DataLoader.hpp>
#include <mosqueeze/viz/DashboardGenerator.hpp>
#include <mosqueeze/viz/MarkdownExporter.hpp>
#include <mosqueeze/viz/ComparisonEngine.hpp>

#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>

int main(int argc, char* argv[]) {
    CLI::App app{"MoSqueeze Benchmark Visualization Tool", "mosqueeze-viz"};
    
    std::filesystem::path inputFile;
    std::filesystem::path outputFile;
    std::string format = "html";
    std::filesystem::path compareFile;
    bool diffOnly = false;
    double threshold = 5.0;
    bool verbose = false;
    
    app.add_option("-i,--input", inputFile, "Input results file")
        ->default_val("benchmarks/results/results.json");
    app.add_option("-o,--output", outputFile, "Output file path")
        ->default_val("dashboard.html");
    app.add_option("-f,--format", format, "Output format (html, markdown, json-summary)")
        ->default_val("html");
    app.add_option("--compare", compareFile, "Compare with baseline results");
    app.add_flag("--diff-only", diffOnly, "Show only differences");
    app.add_option("--threshold", threshold, "Min % difference to highlight")
        ->default_val("5.0");
    app.add_flag("-v,--verbose", verbose, "Verbose output");
    
    CLI11_PARSE(app, argc, argv);
    
    try {
        // Load results
        auto results = mosqueeze::viz::DataLoader::load(inputFile);
        
        if (verbose) {
            std::cout << "Loaded " << results.size() << " benchmark results from " 
                      << inputFile << "\n";
        }
        
        // Comparison mode
        if (!compareFile.empty()) {
            auto baseline = mosqueeze::viz::DataLoader::load(compareFile);
            auto comparisons = mosqueeze::viz::ComparisonEngine::compare(
                results, baseline, threshold);
            
            auto filtered = diffOnly 
                ? mosqueeze::viz::ComparisonEngine::filterSignificant(comparisons, threshold)
                : comparisons;
            
            if (format == "html") {
                mosqueeze::viz::DashboardGenerator::generateComparison(filtered, outputFile);
            } else if (format == "markdown") {
                std::cout << mosqueeze::viz::MarkdownExporter::exportComparison(filtered, threshold);
            }
            
            if (verbose) {
                auto regressions = mosqueeze::viz::ComparisonEngine::findRegressions(comparisons);
                std::cout << "Found " << regressions.size() << " regressions\n";
            }
        } else {
            // Single results mode
            if (format == "html") {
                mosqueeze::viz::DashboardGenerator::generate(results, outputFile);
            } else if (format == "markdown") {
                auto md = mosqueeze::viz::MarkdownExporter::exportSummary(results);
                if (outputFile.empty()) {
                    std::cout << md;
                } else {
                    std::ofstream out(outputFile);
                    out << md;
                }
            } else if (format == "json-summary") {
                // JSON summary for scripting
                nlohmann::json summary = mosqueeze::viz::computeSummary(results);
                std::cout << summary.dump(2);
            }
        }
        
        if (verbose) {
            std::cout << "Output written to " << outputFile << "\n";
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
```

---

## Testing

### Unit Tests

Location: `tests/unit/viz/`

```cpp
// DataLoader_test.cpp
TEST(DataLoader, LoadJsonResults) {
    auto rows = DataLoader::loadJson("test_data/results.json");
    EXPECT_EQ(rows.size(), 100);
    EXPECT_DOUBLE_EQ(rows[0].ratio(), 2.45);
}

TEST(DataLoader, LoadCsvResults) {
    auto rows = DataLoader::loadCsv("test_data/results.csv");
    EXPECT_EQ(rows.size(), 50);
}

TEST(DataLoader, AutoDetectFormat) {
    auto rows1 = DataLoader::load("test_data/results.json");
    auto rows2 = DataLoader::load("test_data/results.csv");
    auto rows3 = DataLoader::load("test_data/results.sqlite3");
    EXPECT_EQ(rows1.size(), rows2.size());
}

// ComparisonEngine_test.cpp
TEST(ComparisonEngine, DetectRegressions) {
    auto current = DataLoader::load("test_data/current.json");
    auto baseline = DataLoader::load("test_data/baseline.json");
    auto comparisons = ComparisonEngine::compare(current, baseline, 5.0);
    
    auto regressions = ComparisonEngine::findRegressions(comparisons);
    EXPECT_EQ(regressions.size(), 2);  // zstd/22 had ratio drop
}

TEST(ComparisonEngine, ThresholdFiltering) {
    auto comparisons = ComparisonEngine::compare(current, baseline, 10.0);
    // Only differences > 10% should be flagged
    for (const auto& c : comparisons) {
        if (c.isRegression || c.isImprovement) {
            EXPECT_GE(std::abs(c.ratioDeltaPct), 10.0);
        }
    }
}
```

### Integration Tests

```cpp
// DashboardGenerator_test.cpp
TEST(DashboardGenerator, GenerateHtmlEndToEnd) {
    auto results = DataLoader::load("test_data/full_results.json");
    
    std::filesystem::path outputPath = "test_output/dashboard.html";
    DashboardGenerator::generate(results, outputPath);
    
    ASSERT_TRUE(std::filesystem::exists(outputPath));
    
    std::ifstream in(outputPath);
    std::string content((std::istreambuf_iterator<char>(in)), {});
    
    // Check key elements
    EXPECT_TRUE(content.contains("<title>MoSqueeze Benchmark Results</title>"));
    EXPECT_TRUE(content.contains("Plotly.newPlot"));
    EXPECT_TRUE(content.contains("zstd"));
}
```

---

## CMakeLists.txt

```cmake
# src/mosqueeze-viz/CMakeLists.txt

add_executable(mosqueeze-viz
    src/main.cpp
    src/DataLoader.cpp
    src/DashboardGenerator.cpp
    src/MarkdownExporter.cpp
    src/ComparisonEngine.cpp
    src/Charts.cpp
)

target_include_directories(mosqueeze-viz PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/libmosqueeze/include
    ${CMAKE_SOURCE_DIR}/src/mosqueeze-bench/include
)

target_link_libraries(mosqueeze-viz PRIVATE
    mosqueeze::libmosqueeze
    mosqueeze::bench
    nlohmann_json::nlohmann_json
    sqlite3::sqlite3
    CLI11::CLI11
)

install(TARGETS mosqueeze-viz
    RUNTIME DESTINATION bin
)
```

---

## Success Criteria

- [ ] `mosqueeze-viz -i results.json -o dashboard.html` produces valid HTML
- [ ] Dashboard displays summary statistics correctly
- [ ] Scatter plot (ratio vs encode time) renders correctly
- [ ] `--compare` mode highlights regressions/improvements
- [ ] `--format markdown` outputs valid Markdown tables
- [ ] `--format json-summary` outputs parseable JSON
- [ ] All unit tests pass
- [ ] Integration test generates same output as reference
- [ ] Works with JSON, CSV, and SQLite inputs
- [ ] Works offline (except for Plotly CDN in HTML, with `--no-charts` fallback)

---

## Estimated Effort

| Component | Lines of Code | Hours |
|-----------|---------------|-------|
| DataLoader | ~150 | 4 |
| ComparisonEngine | ~200 | 5 |
| DashboardGenerator | ~400 | 8 |
| MarkdownExporter | ~150 | 3 |
| Charts.cpp | ~200 | 4 |
| CLI main.cpp | ~100 | 2 |
| Tests | ~300 | 6 |
| **Total** | ~1500 | **32** |

**Estimated: 4-5 days**

---

## Related Issues

- Issue #23 — Enhanced benchmark tool (source of SQLite/JSON output)
- Issue #34 — ThreadPool deadlock fix (benchmarks now work correctly)