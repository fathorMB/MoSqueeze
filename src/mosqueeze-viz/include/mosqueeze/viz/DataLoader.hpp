#pragma once

#include <mosqueeze/viz/Types.hpp>

#include <filesystem>
#include <vector>

namespace mosqueeze::viz {

class DataLoader {
public:
    static std::vector<BenchmarkRow> loadJson(const std::filesystem::path& path);
    static std::vector<BenchmarkRow> loadCsv(const std::filesystem::path& path);
    static std::vector<BenchmarkRow> loadSqlite(const std::filesystem::path& path);
    static std::vector<BenchmarkRow> load(const std::filesystem::path& path);
};

} // namespace mosqueeze::viz
