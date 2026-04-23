#include <mosqueeze/BenchmarkDatabase.hpp>

#include <sqlite3.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace mosqueeze {
namespace {

bool exec(sqlite3* db, const char* sql) {
    char* error = nullptr;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &error);
    if (error != nullptr) {
        sqlite3_free(error);
    }
    return rc == SQLITE_OK;
}

std::string orderClause(OptimizationGoal goal) {
    switch (goal) {
        case OptimizationGoal::Fastest:
            return " ORDER BY expected_time_ms ASC, expected_ratio ASC, sample_count DESC ";
        case OptimizationGoal::Balanced:
            return " ORDER BY (expected_ratio * 0.75 + (expected_time_ms / 10000.0) * 0.25) ASC, sample_count DESC ";
        case OptimizationGoal::MinMemory:
            return " ORDER BY expected_time_ms ASC, expected_ratio ASC, sample_count DESC ";
        case OptimizationGoal::BestDecompression:
            return " ORDER BY expected_time_ms ASC, expected_ratio ASC, sample_count DESC ";
        case OptimizationGoal::MinSize:
        default:
            return " ORDER BY expected_ratio ASC, expected_time_ms ASC, sample_count DESC ";
    }
}

QueryResult fromStmt(sqlite3_stmt* stmt, std::string source) {
    QueryResult out{};
    const unsigned char* preproc = sqlite3_column_text(stmt, 0);
    const unsigned char* algo = sqlite3_column_text(stmt, 1);
    out.preprocessor = preproc != nullptr ? reinterpret_cast<const char*>(preproc) : "none";
    out.algorithm = algo != nullptr ? reinterpret_cast<const char*>(algo) : "zstd";
    out.level = sqlite3_column_int(stmt, 2);
    out.expectedRatio = sqlite3_column_double(stmt, 3);
    out.expectedTimeMs = sqlite3_column_double(stmt, 4);
    const sqlite3_int64 count = sqlite3_column_int64(stmt, 5);
    out.sampleCount = count > 0 ? static_cast<size_t>(count) : 0;
    out.source = std::move(source);
    return out;
}

} // namespace

BenchmarkDatabase::~BenchmarkDatabase() {
    close();
}

BenchmarkDatabase::BenchmarkDatabase(BenchmarkDatabase&& other) noexcept : db_(other.db_) {
    other.db_ = nullptr;
}

BenchmarkDatabase& BenchmarkDatabase::operator=(BenchmarkDatabase&& other) noexcept {
    if (this != &other) {
        close();
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

bool BenchmarkDatabase::open(const std::filesystem::path& dbPath) {
    close();

    sqlite3* handle = nullptr;
    const int rc = sqlite3_open_v2(
        dbPath.string().c_str(),
        &handle,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
        nullptr);
    if (rc != SQLITE_OK || handle == nullptr) {
        if (handle != nullptr) {
            sqlite3_close(handle);
        }
        return false;
    }

    db_ = handle;
    if (!initializeSchema()) {
        close();
        return false;
    }
    return true;
}

void BenchmarkDatabase::close() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool BenchmarkDatabase::initializeSchema() {
    static constexpr const char* kSchema = R"SQL(
CREATE TABLE IF NOT EXISTS benchmark_patterns (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    mime_type TEXT,
    extension TEXT,
    preprocessor TEXT NOT NULL,
    algorithm TEXT NOT NULL,
    level INTEGER NOT NULL,
    expected_ratio REAL NOT NULL,
    expected_time_ms REAL NOT NULL DEFAULT 0,
    sample_count INTEGER NOT NULL DEFAULT 1
);
CREATE INDEX IF NOT EXISTS idx_bench_patterns_mime ON benchmark_patterns(mime_type);
CREATE INDEX IF NOT EXISTS idx_bench_patterns_ext ON benchmark_patterns(extension);

CREATE TABLE IF NOT EXISTS learning_results (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_key TEXT NOT NULL,
    mime_type TEXT,
    extension TEXT,
    preprocessor TEXT NOT NULL,
    algorithm TEXT NOT NULL,
    level INTEGER NOT NULL,
    compressed_size INTEGER NOT NULL,
    original_size INTEGER NOT NULL,
    compression_time_ms INTEGER NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_learning_mime ON learning_results(mime_type);
CREATE INDEX IF NOT EXISTS idx_learning_ext ON learning_results(extension);
CREATE INDEX IF NOT EXISTS idx_learning_file ON learning_results(file_key);
)SQL";

    return exec(db_, kSchema);
}

std::vector<QueryResult> BenchmarkDatabase::query(
    const FileFeatures& features,
    OptimizationGoal goal,
    size_t limit) const {
    if (db_ == nullptr || limit == 0) {
        return {};
    }

    std::vector<QueryResult> merged = queryPatternTable(features, goal, limit);
    const std::vector<QueryResult> learned = queryLearningTable(features, goal, limit);
    merged.insert(merged.end(), learned.begin(), learned.end());

    std::unordered_map<std::string, QueryResult> dedup;
    dedup.reserve(merged.size());
    for (auto& item : merged) {
        const std::string key = item.preprocessor + "|" + item.algorithm + "|" + std::to_string(item.level);
        const auto it = dedup.find(key);
        if (it == dedup.end() || item.expectedRatio < it->second.expectedRatio) {
            dedup[key] = std::move(item);
        }
    }

    std::vector<QueryResult> out;
    out.reserve(dedup.size());
    for (auto& [key, value] : dedup) {
        (void)key;
        out.push_back(std::move(value));
    }

    const std::string order = orderClause(goal);
    std::sort(out.begin(), out.end(), [goal](const QueryResult& lhs, const QueryResult& rhs) {
        if (goal == OptimizationGoal::Fastest || goal == OptimizationGoal::BestDecompression ||
            goal == OptimizationGoal::MinMemory) {
            if (lhs.expectedTimeMs != rhs.expectedTimeMs) {
                return lhs.expectedTimeMs < rhs.expectedTimeMs;
            }
            return lhs.expectedRatio < rhs.expectedRatio;
        }
        if (goal == OptimizationGoal::Balanced) {
            const double left = lhs.expectedRatio * 0.75 + (lhs.expectedTimeMs / 10000.0) * 0.25;
            const double right = rhs.expectedRatio * 0.75 + (rhs.expectedTimeMs / 10000.0) * 0.25;
            if (left != right) {
                return left < right;
            }
            return lhs.sampleCount > rhs.sampleCount;
        }
        if (lhs.expectedRatio != rhs.expectedRatio) {
            return lhs.expectedRatio < rhs.expectedRatio;
        }
        return lhs.expectedTimeMs < rhs.expectedTimeMs;
    });

    if (out.size() > limit) {
        out.resize(limit);
    }
    return out;
}

std::vector<QueryResult> BenchmarkDatabase::queryPatternTable(
    const FileFeatures& features,
    OptimizationGoal goal,
    size_t limit) const {
    std::vector<QueryResult> results;
    if (db_ == nullptr) {
        return results;
    }

    std::string sql =
        "SELECT preprocessor, algorithm, level, expected_ratio, expected_time_ms, sample_count "
        "FROM benchmark_patterns "
        "WHERE (mime_type = ?1 AND ?1 <> '') OR (extension = ?2 AND ?2 <> '')";
    sql += orderClause(goal);
    sql += " LIMIT ?3";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK || stmt == nullptr) {
        if (stmt != nullptr) {
            sqlite3_finalize(stmt);
        }
        return results;
    }

    sqlite3_bind_text(stmt, 1, features.detectedType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, features.extension.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(limit));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(fromStmt(stmt, "benchmark"));
    }

    sqlite3_finalize(stmt);
    return results;
}

std::vector<QueryResult> BenchmarkDatabase::queryLearningTable(
    const FileFeatures& features,
    OptimizationGoal goal,
    size_t limit) const {
    std::vector<QueryResult> results;
    if (db_ == nullptr) {
        return results;
    }

    std::string sql =
        "SELECT preprocessor, algorithm, level, "
        "AVG(CAST(compressed_size AS REAL) / NULLIF(original_size, 0)) AS expected_ratio, "
        "AVG(compression_time_ms) AS expected_time_ms, "
        "COUNT(*) AS sample_count "
        "FROM learning_results "
        "WHERE ((mime_type = ?1 AND ?1 <> '') OR (extension = ?2 AND ?2 <> '')) "
        "GROUP BY preprocessor, algorithm, level";
    sql += orderClause(goal);
    sql += " LIMIT ?3";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK || stmt == nullptr) {
        if (stmt != nullptr) {
            sqlite3_finalize(stmt);
        }
        return results;
    }

    sqlite3_bind_text(stmt, 1, features.detectedType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, features.extension.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(limit));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(fromStmt(stmt, "learning"));
    }

    sqlite3_finalize(stmt);
    return results;
}

bool BenchmarkDatabase::recordResult(
    const std::string& fileKey,
    const FileFeatures& features,
    const std::string& preprocessor,
    const std::string& algorithm,
    int level,
    size_t compressedSize,
    size_t originalSize,
    std::chrono::milliseconds compressionTime) {
    if (db_ == nullptr || originalSize == 0) {
        return false;
    }

    static constexpr const char* kInsertSql =
        "INSERT INTO learning_results("
        "file_key, mime_type, extension, preprocessor, algorithm, level, compressed_size, original_size, compression_time_ms"
        ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kInsertSql, -1, &stmt, nullptr) != SQLITE_OK || stmt == nullptr) {
        if (stmt != nullptr) {
            sqlite3_finalize(stmt);
        }
        return false;
    }

    sqlite3_bind_text(stmt, 1, fileKey.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, features.detectedType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, features.extension.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, preprocessor.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, algorithm.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, level);
    sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(compressedSize));
    sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(originalSize));
    sqlite3_bind_int64(stmt, 9, static_cast<sqlite3_int64>(compressionTime.count()));

    const int stepRc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return stepRc == SQLITE_DONE;
}

} // namespace mosqueeze
