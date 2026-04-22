#include <mosqueeze/viz/ComparisonEngine.hpp>
#include <mosqueeze/viz/DashboardGenerator.hpp>
#include <mosqueeze/viz/DataLoader.hpp>
#include <mosqueeze/viz/MarkdownExporter.hpp>

#include <sqlite3.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        return false;
    }
    return true;
}

void writeText(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}

void createSqliteFixture(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);

    sqlite3* db = nullptr;
    if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
        throw std::runtime_error("Failed to create sqlite fixture");
    }
    const char* createSql =
        "CREATE TABLE IF NOT EXISTS benchmark_results ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "algorithm TEXT NOT NULL,"
        "level INTEGER NOT NULL,"
        "file TEXT NOT NULL,"
        "file_type INTEGER NOT NULL,"
        "original_bytes INTEGER NOT NULL,"
        "compressed_bytes INTEGER NOT NULL,"
        "encode_ms INTEGER NOT NULL,"
        "decode_ms INTEGER NOT NULL,"
        "peak_memory_bytes INTEGER NOT NULL"
        ");";
    char* err = nullptr;
    if (sqlite3_exec(db, createSql, nullptr, nullptr, &err) != SQLITE_OK) {
        const std::string msg = err != nullptr ? err : "sqlite create failed";
        if (err != nullptr) {
            sqlite3_free(err);
        }
        sqlite3_close(db);
        throw std::runtime_error(msg);
    }
    if (err != nullptr) {
        sqlite3_free(err);
    }
    const char* insertSql =
        "INSERT INTO benchmark_results "
        "(algorithm, level, file, file_type, original_bytes, compressed_bytes, encode_ms, decode_ms, peak_memory_bytes) "
        "VALUES ('zstd', 22, 'a.bin', 0, 1000, 500, 10, 4, 1024);";
    if (sqlite3_exec(db, insertSql, nullptr, nullptr, &err) != SQLITE_OK) {
        const std::string msg = err != nullptr ? err : "sqlite insert failed";
        if (err != nullptr) {
            sqlite3_free(err);
        }
        sqlite3_close(db);
        throw std::runtime_error(msg);
    }
    if (err != nullptr) {
        sqlite3_free(err);
    }
    sqlite3_close(db);
}

} // namespace

int main() {
    try {
        const std::filesystem::path testDir = "viz_test_data";
        std::filesystem::create_directories(testDir);

    const std::string jsonPayload = R"([
  {"algorithm":"zstd","level":22,"file":"a.bin","fileType":0,"originalBytes":1000,"compressedBytes":500,"encodeMs":10,"decodeMs":4,"peakMemoryBytes":1024},
  {"algorithm":"lzma","level":9,"file":"a.bin","fileType":0,"originalBytes":1000,"compressedBytes":450,"encodeMs":30,"decodeMs":8,"peakMemoryBytes":2048}
])";
    writeText(testDir / "results.json", jsonPayload);

    const std::string csvPayload =
        "algorithm,level,file,file_type,original_bytes,compressed_bytes,encode_ms,decode_ms,peak_memory_bytes,ratio\n"
        "zstd,22,\"a.bin\",0,1000,500,10,4,1024,2\n"
        "lzma,9,\"a.bin\",0,1000,450,30,8,2048,2.222\n";
    writeText(testDir / "results.csv", csvPayload);

    createSqliteFixture(testDir / "results.sqlite3");

    auto fromJson = mosqueeze::viz::DataLoader::load(testDir / "results.json");
    auto fromCsv = mosqueeze::viz::DataLoader::load(testDir / "results.csv");
    auto fromSqlite = mosqueeze::viz::DataLoader::load(testDir / "results.sqlite3");
    if (!expect(fromJson.size() == 2, "JSON loader should produce 2 rows")) {
        return 1;
    }
    if (!expect(fromCsv.size() == 2, "CSV loader should produce 2 rows")) {
        return 1;
    }
    if (!expect(fromSqlite.size() == 1, "SQLite loader should produce 1 row")) {
        return 1;
    }

    const std::string baselineJson = R"([
  {"algorithm":"zstd","level":22,"file":"a.bin","fileType":0,"originalBytes":1000,"compressedBytes":520,"encodeMs":9,"decodeMs":4,"peakMemoryBytes":1024},
  {"algorithm":"lzma","level":9,"file":"a.bin","fileType":0,"originalBytes":1000,"compressedBytes":440,"encodeMs":35,"decodeMs":9,"peakMemoryBytes":2048}
])";
    writeText(testDir / "baseline.json", baselineJson);
    auto baseline = mosqueeze::viz::DataLoader::load(testDir / "baseline.json");

    auto comparisons = mosqueeze::viz::ComparisonEngine::compare(fromJson, baseline, 5.0);
    if (!expect(!comparisons.empty(), "Comparison results should not be empty")) {
        return 1;
    }
    auto significant = mosqueeze::viz::ComparisonEngine::filterSignificant(comparisons, 5.0);
    if (!expect(!significant.empty(), "Significant comparisons should not be empty")) {
        return 1;
    }

    const auto markdown = mosqueeze::viz::MarkdownExporter::exportSummary(fromJson, "Viz Test");
    if (!expect(markdown.find("# Viz Test") != std::string::npos, "Markdown title missing")) {
        return 1;
    }
    if (!expect(markdown.find("| Algorithm |") != std::string::npos, "Markdown summary table missing")) {
        return 1;
    }

    const auto comparisonMarkdown = mosqueeze::viz::MarkdownExporter::exportComparison(comparisons, 5.0);
    if (!expect(comparisonMarkdown.find("# Benchmark Comparison") != std::string::npos,
                "Comparison markdown title missing")) {
        return 1;
    }

    mosqueeze::viz::DashboardOptions options{};
    options.noCharts = true;
    mosqueeze::viz::DashboardGenerator::generate(fromJson, testDir / "dashboard.html", options);
    mosqueeze::viz::DashboardGenerator::generateComparison(comparisons, testDir / "comparison.html", options);

        {
            std::ifstream html(testDir / "dashboard.html", std::ios::binary);
            const std::string htmlContent((std::istreambuf_iterator<char>(html)), std::istreambuf_iterator<char>());
            if (!expect(htmlContent.find("<title>MoSqueeze Benchmark Results</title>") != std::string::npos,
                        "Dashboard HTML title missing")) {
                return 1;
            }
        }

        std::filesystem::remove_all(testDir);
        std::cout << "[PASS] Viz_test\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] Viz_test exception: " << ex.what() << "\n";
        return 1;
    }
}
