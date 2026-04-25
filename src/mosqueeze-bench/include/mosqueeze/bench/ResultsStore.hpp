#pragma once

#include <mosqueeze/bench/BenchmarkRunner.hpp>

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

struct sqlite3;

namespace mosqueeze::bench {

class ResultsStore {
public:
    explicit ResultsStore(const std::filesystem::path& dbPath = ":memory:");
    ~ResultsStore();

    ResultsStore(const ResultsStore&) = delete;
    ResultsStore& operator=(const ResultsStore&) = delete;

    ResultsStore(ResultsStore&&) noexcept = delete;
    ResultsStore& operator=(ResultsStore&&) noexcept = delete;

    void save(const BenchmarkResult& result);
    void saveAll(const std::vector<BenchmarkResult>& results);

    std::vector<BenchmarkResult> query(const std::string& whereClause = "");

    void exportCsv(const std::filesystem::path& output);
    void exportJson(const std::filesystem::path& output, bool interrupted = false);
    std::unordered_set<std::string> loadExistingKeys();

    void clear();

private:
    std::filesystem::path dbPath_;
    sqlite3* db_ = nullptr;

    void initDatabase();
};

} // namespace mosqueeze::bench
