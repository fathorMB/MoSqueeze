#pragma once

#include <mosqueeze/bench/BenchmarkConfig.hpp>
#include <mosqueeze/bench/BenchmarkResult.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace mosqueeze::bench::Formatters {

std::string formatVerbose(
    const BenchmarkResult& result,
    const BenchmarkStats* stats = nullptr);

std::string formatSummaryTable(
    const std::vector<BenchmarkResult>& results,
    const std::unordered_map<std::string, BenchmarkStats>* stats = nullptr);

std::string exportMarkdown(
    const std::vector<BenchmarkResult>& results,
    const std::unordered_map<std::string, BenchmarkStats>* stats = nullptr);

std::string exportHtml(
    const std::vector<BenchmarkResult>& results,
    const std::unordered_map<std::string, BenchmarkStats>* stats = nullptr);

} // namespace mosqueeze::bench::Formatters
