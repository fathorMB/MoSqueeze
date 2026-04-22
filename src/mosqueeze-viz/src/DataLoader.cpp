#include <mosqueeze/viz/DataLoader.hpp>

#include <nlohmann/json.hpp>
#include <sqlite3.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mosqueeze::viz {
namespace {

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::vector<std::string> parseCsvLine(const std::string& line) {
    std::vector<std::string> out;
    std::string current;
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
            continue;
        }
        if (ch == ',' && !inQuotes) {
            out.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    out.push_back(current);
    return out;
}

BenchmarkRow parseJsonRow(const nlohmann::json& row) {
    BenchmarkRow out{};
    out.algorithm = row.value("algorithm", "");
    out.level = row.value("level", 0);
    out.file = row.value("file", "");
    out.fileType = row.value("fileType", 0);
    out.originalBytes = row.value("originalBytes", static_cast<size_t>(0));
    out.compressedBytes = row.value("compressedBytes", static_cast<size_t>(0));
    out.encodeTime = std::chrono::milliseconds(row.value("encodeMs", 0));
    out.decodeTime = std::chrono::milliseconds(row.value("decodeMs", 0));
    out.peakMemoryBytes = row.value("peakMemoryBytes", static_cast<size_t>(0));
    return out;
}

} // namespace

std::vector<BenchmarkRow> DataLoader::loadJson(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open JSON input: " + path.string());
    }

    nlohmann::json payload;
    in >> payload;
    if (!payload.is_array()) {
        throw std::runtime_error("Benchmark JSON input must be an array");
    }

    std::vector<BenchmarkRow> rows;
    rows.reserve(payload.size());
    for (const auto& row : payload) {
        rows.push_back(parseJsonRow(row));
    }
    return rows;
}

std::vector<BenchmarkRow> DataLoader::loadCsv(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open CSV input: " + path.string());
    }

    std::string line;
    if (!std::getline(in, line)) {
        return {};
    }

    std::vector<BenchmarkRow> rows;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const auto cols = parseCsvLine(line);
        if (cols.size() < 9) {
            continue;
        }

        BenchmarkRow row{};
        row.algorithm = cols[0];
        row.level = std::stoi(cols[1]);
        row.file = cols[2];
        row.fileType = std::stoi(cols[3]);
        row.originalBytes = static_cast<size_t>(std::stoll(cols[4]));
        row.compressedBytes = static_cast<size_t>(std::stoll(cols[5]));
        row.encodeTime = std::chrono::milliseconds(std::stoll(cols[6]));
        row.decodeTime = std::chrono::milliseconds(std::stoll(cols[7]));
        row.peakMemoryBytes = static_cast<size_t>(std::stoll(cols[8]));
        rows.push_back(std::move(row));
    }
    return rows;
}

std::vector<BenchmarkRow> DataLoader::loadSqlite(const std::filesystem::path& path) {
    sqlite3* db = nullptr;
    if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) {
        const std::string message = db != nullptr ? sqlite3_errmsg(db) : "sqlite3_open failed";
        if (db != nullptr) {
            sqlite3_close(db);
        }
        throw std::runtime_error("Failed to open sqlite input: " + message);
    }

    struct DbGuard {
        sqlite3* db;
        ~DbGuard() { sqlite3_close(db); }
    } guard{db};

    const char* sql =
        "SELECT algorithm, level, file, file_type, original_bytes, compressed_bytes, encode_ms, decode_ms, peak_memory_bytes "
        "FROM benchmark_results;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(std::string("sqlite3_prepare_v2 failed: ") + sqlite3_errmsg(db));
    }

    struct StatementGuard {
        sqlite3_stmt* stmt;
        ~StatementGuard() { sqlite3_finalize(stmt); }
    } statementGuard{stmt};

    std::vector<BenchmarkRow> rows;
    while (true) {
        const int rc = sqlite3_step(stmt);
        if (rc == SQLITE_DONE) {
            break;
        }
        if (rc != SQLITE_ROW) {
            throw std::runtime_error(std::string("sqlite3_step failed: ") + sqlite3_errmsg(db));
        }

        BenchmarkRow row{};
        row.algorithm = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        row.level = sqlite3_column_int(stmt, 1);
        row.file = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        row.fileType = sqlite3_column_int(stmt, 3);
        row.originalBytes = static_cast<size_t>(sqlite3_column_int64(stmt, 4));
        row.compressedBytes = static_cast<size_t>(sqlite3_column_int64(stmt, 5));
        row.encodeTime = std::chrono::milliseconds(sqlite3_column_int64(stmt, 6));
        row.decodeTime = std::chrono::milliseconds(sqlite3_column_int64(stmt, 7));
        row.peakMemoryBytes = static_cast<size_t>(sqlite3_column_int64(stmt, 8));
        rows.push_back(std::move(row));
    }

    return rows;
}

std::vector<BenchmarkRow> DataLoader::load(const std::filesystem::path& path) {
    const std::string ext = toLower(path.extension().string());
    if (ext == ".json") {
        return loadJson(path);
    }
    if (ext == ".csv") {
        return loadCsv(path);
    }
    if (ext == ".sqlite3" || ext == ".db") {
        return loadSqlite(path);
    }
    throw std::runtime_error("Unsupported input format: " + path.string());
}

} // namespace mosqueeze::viz
