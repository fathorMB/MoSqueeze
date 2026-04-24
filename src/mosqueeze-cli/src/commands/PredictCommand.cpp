#include "PredictCommand.hpp"

#include <mosqueeze/PredictionEngine.hpp>

#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mosqueeze::cli {

namespace {

// ---------------------------------------------------------------------------
// Text formatter
// ---------------------------------------------------------------------------

std::string formatText(const PredictionResult& r) {
    std::string out;

    out += fmt::format("File: {}\n", r.path.string());
    out += fmt::format("Size: {} bytes\n", r.inputSize);
    out += fmt::format("Type: {}\n", r.mimeType.empty() ? "unknown" : r.mimeType);

    if (r.shouldSkip) {
        out += fmt::format("\nSKIP: {}\n", r.skipReason);
        return out;
    }

    if (r.preprocessorAvailable)
        out += fmt::format("Preprocessor: {}\n", r.preprocessor);

    out += "\nRecommendations:\n";

    int i = 1;
    for (const auto& opt : r.recommendations) {
        const char* speedTag =
            opt.speedLabel == "fast"   ? "[fast]"   :
            opt.speedLabel == "medium" ? "[medium]" : "[slow]";

        out += fmt::format("  {}. {} level {} {} {:.1f}x ratio\n",
                           i++, opt.algorithm, opt.level,
                           speedTag, opt.estimatedRatio);

        if (r.inputSize > 0 && opt.estimatedSize > 0) {
            const double pct = 100.0 * (1.0 - opt.estimatedSize /
                                        static_cast<double>(r.inputSize));
            out += fmt::format("     Estimated size: {} bytes ({:.1f}% compression)\n",
                               opt.estimatedSize, pct);
        }

        if (!opt.fallbackAlgorithm.empty()) {
            out += fmt::format("     Fallback: {} level {} ({:.1f}x)\n",
                               opt.fallbackAlgorithm, opt.fallbackLevel,
                               opt.fallbackRatio);
        }
    }

    return out;
}

// ---------------------------------------------------------------------------
// Multi-file JSON output
// ---------------------------------------------------------------------------

std::string formatJsonBatch(const std::vector<PredictionResult>& results, int indent) {
    if (results.size() == 1)
        return results[0].toJson(indent);

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& r : results)
        arr.push_back(nlohmann::json::parse(r.toJson(-1)));
    return arr.dump(indent);
}

std::string formatTextBatch(const std::vector<PredictionResult>& results) {
    std::string out;
    for (size_t i = 0; i < results.size(); ++i) {
        if (i > 0) out += "\n---\n\n";
        out += formatText(results[i]);
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// Command registration
// ---------------------------------------------------------------------------

void addPredictCommand(CLI::App& app) {
    auto* predict = app.add_subcommand("predict",
        "Predict compression options for one or more files");

    auto files         = std::make_shared<std::vector<std::string>>();
    auto outputFile    = std::make_shared<std::string>();
    auto format        = std::make_shared<std::string>("text");
    auto preferSpeed   = std::make_shared<bool>(false);
    auto matrixPath    = std::make_shared<std::string>();
    auto verbose       = std::make_shared<bool>(false);

    predict->add_option("files", *files, "File(s) to analyze")->required();
    predict->add_option("-o,--output", *outputFile,
        "Write output to file instead of stdout");
    predict->add_option("-f,--format", *format, "Output format: text (default), json")
        ->check(CLI::IsMember({"text", "json"}));
    predict->add_flag("-s,--prefer-speed", *preferSpeed,
        "Prioritize fast algorithms over best ratio");
    predict->add_option("--decision-matrix", *matrixPath,
        "Path to custom decision matrix JSON (default: embedded)");
    predict->add_flag("-v,--verbose", *verbose,
        "Show debug information");

    predict->callback([=]() {
        namespace fs = std::filesystem;

        // Validate all inputs exist before doing any work
        for (const auto& f : *files) {
            if (!fs::exists(f))
                throw std::runtime_error("File does not exist: " + f);
        }

        PredictionConfig cfg;
        cfg.preferSpeed = *preferSpeed;
        if (!matrixPath->empty())
            cfg.decisionMatrixPath = *matrixPath;

        PredictionEngine engine;
        engine.setConfig(cfg);

        ProgressCallback progress;
        if (*verbose) {
            progress = [](const std::string& stage, double pct) {
                spdlog::debug("  [predict] {} {:.0f}%", stage, pct * 100.0);
            };
        }

        // Collect results
        std::vector<PredictionResult> results;
        results.reserve(files->size());
        for (const auto& f : *files)
            results.push_back(engine.predict(fs::path(f), progress));

        // Format
        const std::string output =
            *format == "json"
                ? formatJsonBatch(results, 2)
                : formatTextBatch(results);

        // Emit
        if (outputFile->empty()) {
            fmt::print("{}\n", output);
        } else {
            std::ofstream out(*outputFile);
            if (!out)
                throw std::runtime_error("Cannot open output file: " + *outputFile);
            out << output << "\n";
            spdlog::info("Prediction written to {}", *outputFile);
        }
    });
}

} // namespace mosqueeze::cli
