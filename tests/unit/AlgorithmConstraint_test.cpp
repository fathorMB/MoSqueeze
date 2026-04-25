#include <mosqueeze/bench/BenchmarkRunner.hpp>
#include <mosqueeze/engines/ZpaqEngine.hpp>
#include <mosqueeze/engines/ZstdEngine.hpp>

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

static std::filesystem::path createTempFile(size_t size) {
    static int counter = 0;
    auto path = std::filesystem::temp_directory_path() / ("algo_constraint_test_" + std::to_string(++counter) + ".bin");
    std::ofstream out(path, std::ios::binary);
    std::vector<char> data(size, 'A');
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.close();
    return path;
}

static void cleanup(const std::filesystem::path& p) {
    std::filesystem::remove(p);
}

int main() {
    int passed = 0;
    int total = 0;

    // Test 1: ZPAQ levels limited for large files (>2MB)
    {
        ++total;
        mosqueeze::bench::BenchmarkRunner runner;
        runner.registerEngine(std::make_unique<mosqueeze::ZpaqEngine>());

        const auto allLevels = runner.availableLevels("zpaq");
        const auto filtered = runner.filterLevelsBySize("zpaq", allLevels, 12 * 1024 * 1024);

        bool ok = true;
        for (int level : filtered) {
            if (level > 1) {
                ok = false;
                break;
            }
        }
        if (ok && !filtered.empty()) {
            ++passed;
            std::cout << "[PASS] ZpaqLevelsLimitedForLargeFiles\n";
        } else {
            std::cout << "[FAIL] ZpaqLevelsLimitedForLargeFiles: expected only level 1, got ";
            for (int l : filtered) std::cout << l << " ";
            std::cout << "\n";
        }
    }

    // Test 2: ZPAQ all levels for small files (<=2MB)
    {
        ++total;
        mosqueeze::bench::BenchmarkRunner runner;
        runner.registerEngine(std::make_unique<mosqueeze::ZpaqEngine>());

        const auto allLevels = runner.availableLevels("zpaq");
        const auto filtered = runner.filterLevelsBySize("zpaq", allLevels, 500 * 1024);

        if (filtered.size() == allLevels.size()) {
            ++passed;
            std::cout << "[PASS] ZpaqAllLevelsForSmallFiles\n";
        } else {
            std::cout << "[FAIL] ZpaqAllLevelsForSmallFiles: expected " << allLevels.size()
                      << " levels, got " << filtered.size() << "\n";
        }
    }

    // Test 3: ZPAQ exactly at 2MB boundary (should still allow all levels)
    {
        ++total;
        mosqueeze::bench::BenchmarkRunner runner;
        runner.registerEngine(std::make_unique<mosqueeze::ZpaqEngine>());

        const auto allLevels = runner.availableLevels("zpaq");
        const auto filtered = runner.filterLevelsBySize("zpaq", allLevels, 2 * 1024 * 1024);

        if (filtered.size() == allLevels.size()) {
            ++passed;
            std::cout << "[PASS] ZpaqAllLevelsAtBoundary\n";
        } else {
            std::cout << "[FAIL] ZpaqAllLevelsAtBoundary: expected " << allLevels.size()
                      << " levels at exactly 2MB, got " << filtered.size() << "\n";
        }
    }

    // Test 4: Other algorithms are not affected (no constraint registered)
    {
        ++total;
        mosqueeze::bench::BenchmarkRunner runner;
        runner.registerEngine(std::make_unique<mosqueeze::ZstdEngine>());

        const auto allLevels = runner.availableLevels("zstd");
        const auto filtered = runner.filterLevelsBySize("zstd", allLevels, 50 * 1024 * 1024);

        if (filtered.size() == allLevels.size()) {
            ++passed;
            std::cout << "[PASS] OtherAlgorithmsUnaffected\n";
        } else {
            std::cout << "[FAIL] OtherAlgorithmsUnaffected: expected " << allLevels.size()
                      << " levels, got " << filtered.size() << "\n";
        }
    }

    // Test 5: Custom constraint can be registered
    {
        ++total;
        mosqueeze::bench::BenchmarkRunner runner;
        runner.registerEngine(std::make_unique<mosqueeze::ZstdEngine>());
        runner.registerConstraint("zstd", mosqueeze::bench::AlgorithmConstraint{
            .maxFileSize = 1024 * 1024,
            .maxLevelForSize = 3
        });

        const auto allLevels = runner.availableLevels("zstd");
        const auto filtered = runner.filterLevelsBySize("zstd", allLevels, 5 * 1024 * 1024);

        bool ok = true;
        for (int level : filtered) {
            if (level > 3) {
                ok = false;
                break;
            }
        }
        if (ok) {
            ++passed;
            std::cout << "[PASS] CustomConstraintRegistered\n";
        } else {
            std::cout << "[FAIL] CustomConstraintRegistered: expected max level 3\n";
        }
    }

    // Test 6: Constraint with maxFileSize=0 means unlimited
    {
        ++total;
        mosqueeze::bench::BenchmarkRunner runner;
        runner.registerEngine(std::make_unique<mosqueeze::ZpaqEngine>());
        runner.registerConstraint("zpaq", mosqueeze::bench::AlgorithmConstraint{
            .maxFileSize = 0,
            .maxLevelForSize = 1
        });

        const auto allLevels = runner.availableLevels("zpaq");
        const auto filtered = runner.filterLevelsBySize("zpaq", allLevels, 50 * 1024 * 1024);

        if (filtered.size() == allLevels.size()) {
            ++passed;
            std::cout << "[PASS] ZeroMaxFileSizeMeansUnlimited\n";
        } else {
            std::cout << "[FAIL] ZeroMaxFileSizeMeansUnlimited: expected all levels\n";
        }
    }

    std::cout << "\n" << passed << "/" << total << " tests passed\n";
    return (passed == total) ? 0 : 1;
}