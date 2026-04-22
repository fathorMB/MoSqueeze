#include <mosqueeze/bench/ResultsStore.hpp>

#include <sqlite3.h>

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>
#include <string>

namespace mosqueeze::bench {
namespace {

void ensureSqlOk(int rc, sqlite3* db, const char* context) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        throw std::runtime_error(std::string(context) + ": " + sqlite3_errmsg(db));
    }
}

bool tableHasColumn(sqlite3* db, const char* table, const char* column) {
    const std::string sql = "PRAGMA table_info(" + std::string(table) + ");";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    struct StatementGuard {
        sqlite3_stmt* stmt;
        ~StatementGuard() { sqlite3_finalize(stmt); }
    } guard{stmt};

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (name != nullptr && std::string(name) == column) {
            return true;
        }
    }
    return false;
}

void addColumnIfMissing(sqlite3* db, const char* table, const char* column, const char* spec) {
    if (tableHasColumn(db, table, column)) {
        return;
    }

    const std::string sql =
        "ALTER TABLE " + std::string(table) + " ADD COLUMN " + std::string(column) + " " + std::string(spec) + ";";
    char* err = nullptr;
    const int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        const std::string msg = err != nullptr ? err : "unknown sqlite error";
        sqlite3_free(err);
        throw std::runtime_error("Failed to migrate benchmark_results table: " + msg);
    }
}

} // namespace

ResultsStore::ResultsStore(const std::filesystem::path& dbPath)
    : dbPath_(dbPath) {
    const std::string sqlitePath = dbPath_ == std::filesystem::path(":memory:")
        ? ":memory:"
        : dbPath_.string();

    const int rc = sqlite3_open(sqlitePath.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string message = "Failed to open sqlite database";
        if (db_ != nullptr) {
            message += ": ";
            message += sqlite3_errmsg(db_);
        }
        throw std::runtime_error(message);
    }

    initDatabase();
}

ResultsStore::~ResultsStore() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void ResultsStore::initDatabase() {
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
        "peak_memory_bytes INTEGER NOT NULL,"
        "preprocess_type TEXT NOT NULL DEFAULT 'none',"
        "preprocess_original_bytes INTEGER NOT NULL DEFAULT 0,"
        "preprocess_processed_bytes INTEGER NOT NULL DEFAULT 0,"
        "preprocess_time_ms REAL NOT NULL DEFAULT 0,"
        "preprocess_improvement REAL NOT NULL DEFAULT 0,"
        "preprocess_applied INTEGER NOT NULL DEFAULT 0,"
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ");";

    char* err = nullptr;
    const int rc = sqlite3_exec(db_, createSql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        const std::string msg = err != nullptr ? err : "unknown sqlite error";
        sqlite3_free(err);
        throw std::runtime_error("Failed to initialize benchmark_results table: " + msg);
    }

    addColumnIfMissing(db_, "benchmark_results", "preprocess_type", "TEXT NOT NULL DEFAULT 'none'");
    addColumnIfMissing(db_, "benchmark_results", "preprocess_original_bytes", "INTEGER NOT NULL DEFAULT 0");
    addColumnIfMissing(db_, "benchmark_results", "preprocess_processed_bytes", "INTEGER NOT NULL DEFAULT 0");
    addColumnIfMissing(db_, "benchmark_results", "preprocess_time_ms", "REAL NOT NULL DEFAULT 0");
    addColumnIfMissing(db_, "benchmark_results", "preprocess_improvement", "REAL NOT NULL DEFAULT 0");
    addColumnIfMissing(db_, "benchmark_results", "preprocess_applied", "INTEGER NOT NULL DEFAULT 0");
}

void ResultsStore::save(const BenchmarkResult& result) {
    const char* insertSql =
        "INSERT INTO benchmark_results "
        "(algorithm, level, file, file_type, original_bytes, compressed_bytes, encode_ms, decode_ms, peak_memory_bytes, "
        "preprocess_type, preprocess_original_bytes, preprocess_processed_bytes, preprocess_time_ms, preprocess_improvement, preprocess_applied) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    ensureSqlOk(sqlite3_prepare_v2(db_, insertSql, -1, &stmt, nullptr), db_, "sqlite3_prepare_v2(insert)");

    struct StatementGuard {
        sqlite3_stmt* stmt;
        ~StatementGuard() { sqlite3_finalize(stmt); }
    } guard{stmt};

    ensureSqlOk(sqlite3_bind_text(stmt, 1, result.algorithm.c_str(), -1, SQLITE_TRANSIENT), db_, "bind algorithm");
    ensureSqlOk(sqlite3_bind_int(stmt, 2, result.level), db_, "bind level");
    ensureSqlOk(sqlite3_bind_text(stmt, 3, result.file.string().c_str(), -1, SQLITE_TRANSIENT), db_, "bind file");
    ensureSqlOk(sqlite3_bind_int(stmt, 4, static_cast<int>(result.fileType)), db_, "bind file_type");
    ensureSqlOk(sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(result.originalBytes)), db_, "bind original_bytes");
    ensureSqlOk(sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(result.compressedBytes)), db_, "bind compressed_bytes");
    ensureSqlOk(sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(result.encodeTime.count())), db_, "bind encode_ms");
    ensureSqlOk(sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(result.decodeTime.count())), db_, "bind decode_ms");
    ensureSqlOk(sqlite3_bind_int64(stmt, 9, static_cast<sqlite3_int64>(result.peakMemoryBytes)), db_, "bind peak_memory_bytes");
    ensureSqlOk(sqlite3_bind_text(stmt, 10, result.preprocess.type.c_str(), -1, SQLITE_TRANSIENT), db_, "bind preprocess_type");
    ensureSqlOk(sqlite3_bind_int64(stmt, 11, static_cast<sqlite3_int64>(result.preprocess.originalBytes)), db_, "bind preprocess_original_bytes");
    ensureSqlOk(sqlite3_bind_int64(stmt, 12, static_cast<sqlite3_int64>(result.preprocess.processedBytes)), db_, "bind preprocess_processed_bytes");
    ensureSqlOk(sqlite3_bind_double(stmt, 13, result.preprocess.preprocessingTimeMs), db_, "bind preprocess_time_ms");
    ensureSqlOk(sqlite3_bind_double(stmt, 14, result.preprocess.improvement), db_, "bind preprocess_improvement");
    ensureSqlOk(sqlite3_bind_int(stmt, 15, result.preprocess.applied ? 1 : 0), db_, "bind preprocess_applied");

    ensureSqlOk(sqlite3_step(stmt), db_, "sqlite3_step(insert)");
}

void ResultsStore::saveAll(const std::vector<BenchmarkResult>& results) {
    char* err = nullptr;
    ensureSqlOk(sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, &err), db_, "begin transaction");
    if (err != nullptr) {
        sqlite3_free(err);
    }

    try {
        for (const auto& result : results) {
            save(result);
        }
        ensureSqlOk(sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &err), db_, "commit transaction");
        if (err != nullptr) {
            sqlite3_free(err);
        }
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        throw;
    }
}

std::vector<BenchmarkResult> ResultsStore::query(const std::string& whereClause) {
    std::string sql =
        "SELECT algorithm, level, file, file_type, original_bytes, compressed_bytes, encode_ms, decode_ms, peak_memory_bytes, "
        "preprocess_type, preprocess_original_bytes, preprocess_processed_bytes, preprocess_time_ms, preprocess_improvement, preprocess_applied "
        "FROM benchmark_results";

    if (!whereClause.empty()) {
        sql += " WHERE ";
        sql += whereClause;
    }

    sql += ";";

    sqlite3_stmt* stmt = nullptr;
    ensureSqlOk(sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr), db_, "sqlite3_prepare_v2(query)");

    struct StatementGuard {
        sqlite3_stmt* stmt;
        ~StatementGuard() { sqlite3_finalize(stmt); }
    } guard{stmt};

    std::vector<BenchmarkResult> rows;

    while (true) {
        const int rc = sqlite3_step(stmt);
        if (rc == SQLITE_DONE) {
            break;
        }
        ensureSqlOk(rc, db_, "sqlite3_step(query)");

        BenchmarkResult row{};
        row.algorithm = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        row.level = sqlite3_column_int(stmt, 1);
        row.file = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        row.fileType = static_cast<FileType>(sqlite3_column_int(stmt, 3));
        row.originalBytes = static_cast<size_t>(sqlite3_column_int64(stmt, 4));
        row.compressedBytes = static_cast<size_t>(sqlite3_column_int64(stmt, 5));
        row.encodeTime = std::chrono::milliseconds(sqlite3_column_int64(stmt, 6));
        row.decodeTime = std::chrono::milliseconds(sqlite3_column_int64(stmt, 7));
        row.peakMemoryBytes = static_cast<size_t>(sqlite3_column_int64(stmt, 8));
        row.preprocess.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        row.preprocess.originalBytes = static_cast<size_t>(sqlite3_column_int64(stmt, 10));
        row.preprocess.processedBytes = static_cast<size_t>(sqlite3_column_int64(stmt, 11));
        row.preprocess.preprocessingTimeMs = sqlite3_column_double(stmt, 12);
        row.preprocess.improvement = sqlite3_column_double(stmt, 13);
        row.preprocess.applied = sqlite3_column_int(stmt, 14) != 0;

        rows.push_back(std::move(row));
    }

    return rows;
}

void ResultsStore::exportCsv(const std::filesystem::path& output) {
    std::filesystem::create_directories(output.parent_path());

    std::ofstream out(output, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open CSV output path: " + output.string());
    }

    out << "algorithm,level,file,file_type,original_bytes,compressed_bytes,encode_ms,decode_ms,peak_memory_bytes,ratio,total_ratio,"
           "preprocess_type,preprocess_original_bytes,preprocess_processed_bytes,preprocess_time_ms,preprocess_improvement,preprocess_applied\n";
    for (const auto& row : query()) {
        out << row.algorithm << ','
            << row.level << ','
            << '"' << row.file.string() << '"' << ','
            << static_cast<int>(row.fileType) << ','
            << row.originalBytes << ','
            << row.compressedBytes << ','
            << row.encodeTime.count() << ','
            << row.decodeTime.count() << ','
            << row.peakMemoryBytes << ','
            << row.ratio() << ','
            << row.totalRatio() << ','
            << row.preprocess.type << ','
            << row.preprocess.originalBytes << ','
            << row.preprocess.processedBytes << ','
            << row.preprocess.preprocessingTimeMs << ','
            << row.preprocess.improvement << ','
            << (row.preprocess.applied ? 1 : 0) << '\n';
    }
}

void ResultsStore::exportJson(const std::filesystem::path& output) {
    std::filesystem::create_directories(output.parent_path());

    nlohmann::json payload = nlohmann::json::array();
    for (const auto& row : query()) {
        payload.push_back({
            {"algorithm", row.algorithm},
            {"level", row.level},
            {"file", row.file.string()},
            {"fileType", static_cast<int>(row.fileType)},
            {"originalBytes", row.originalBytes},
            {"compressedBytes", row.compressedBytes},
            {"encodeMs", row.encodeTime.count()},
            {"decodeMs", row.decodeTime.count()},
            {"peakMemoryBytes", row.peakMemoryBytes},
            {"ratio", row.ratio()},
            {"totalRatio", row.totalRatio()},
            {"preprocess", {
                {"type", row.preprocess.type},
                {"originalBytes", row.preprocess.originalBytes},
                {"processedBytes", row.preprocess.processedBytes},
                {"timeMs", row.preprocess.preprocessingTimeMs},
                {"improvement", row.preprocess.improvement},
                {"applied", row.preprocess.applied}
            }}
        });
    }

    std::ofstream out(output, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open JSON output path: " + output.string());
    }

    out << payload.dump(2);
}

void ResultsStore::clear() {
    char* err = nullptr;
    const int rc = sqlite3_exec(db_, "DELETE FROM benchmark_results;", nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        const std::string msg = err != nullptr ? err : "unknown sqlite error";
        sqlite3_free(err);
        throw std::runtime_error("Failed to clear benchmark results: " + msg);
    }
}

} // namespace mosqueeze::bench
