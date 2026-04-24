#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Compile-time binary path injected by CMake
// ---------------------------------------------------------------------------

#ifndef MOSQUEEZE_CLI_PATH
#define MOSQUEEZE_CLI_PATH ""
#endif

// ---------------------------------------------------------------------------
// Cross-platform helpers
// ---------------------------------------------------------------------------

// Returns {stdout, exit_code}
static std::pair<std::string, int> runCmd(const std::string& cmd) {
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) throw std::runtime_error("popen failed: " + cmd);

    std::array<char, 256> buf{};
    std::string output;
    while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe))
        output += buf.data();

#ifdef _WIN32
    const int rc = _pclose(pipe);
#else
    const int rc = pclose(pipe);
#endif
    return {output, rc};
}

// Redirect stderr to null so spdlog noise doesn't interfere
static std::string devNull() {
#ifdef _WIN32
    return " 2>NUL";
#else
    return " 2>/dev/null";
#endif
}

static fs::path writeTmp(const std::string& name, const std::string& content) {
    const fs::path p = fs::temp_directory_path() / ("cli_int_test_" + name);
    std::ofstream f(p, std::ios::binary);
    f << content;
    return p;
}

static fs::path writeTmpJpeg(const std::string& name) {
    const fs::path p = fs::temp_directory_path() / ("cli_int_test_" + name);
    std::ofstream f(p, std::ios::binary);
    const unsigned char hdr[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10,
                                  'J', 'F', 'I', 'F', 0x00};
    f.write(reinterpret_cast<const char*>(hdr), sizeof(hdr));
    return p;
}

struct TmpFile {
    fs::path path;
    explicit TmpFile(fs::path p) : path(std::move(p)) {}
    ~TmpFile() { std::error_code ec; fs::remove(path, ec); }
};

// ---------------------------------------------------------------------------
// Guard: skip all CLI tests if binary path is not set / binary missing
// ---------------------------------------------------------------------------

static bool cliBinaryAvailable() {
    const std::string path = MOSQUEEZE_CLI_PATH;
    if (path.empty()) return false;
    std::error_code ec;
    return fs::exists(path, ec) && !ec;
}

#define REQUIRE_CLI() \
    if (!cliBinaryAvailable()) SKIP("mosqueeze binary not available at " MOSQUEEZE_CLI_PATH)

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("CLI predict produces text output for JSON file", "[cli-predict]") {
    REQUIRE_CLI();
    TmpFile f{writeTmp("cli.json", R"({"key":"value","items":[1,2,3]})")};

    const std::string cmd =
        std::string(MOSQUEEZE_CLI_PATH) + " predict \"" +
        f.path.string() + "\"" + devNull();
    auto [output, rc] = runCmd(cmd);

    CHECK(rc == 0);
    CHECK(output.find("File:")            != std::string::npos);
    CHECK(output.find("Recommendations:") != std::string::npos);
}

TEST_CASE("CLI predict produces valid JSON with --format json", "[cli-predict]") {
    REQUIRE_CLI();
    TmpFile f{writeTmp("cli2.json", R"({"test":"data"})")};

    const std::string cmd =
        std::string(MOSQUEEZE_CLI_PATH) + " predict \"" +
        f.path.string() + "\" --format json" + devNull();
    auto [output, rc] = runCmd(cmd);

    CHECK(rc == 0);
    REQUIRE_FALSE(output.empty());

    // Must parse as JSON without throwing
    const auto j = nlohmann::json::parse(output, nullptr, /*exceptions=*/false);
    CHECK_FALSE(j.is_discarded());
    CHECK(j.contains("file"));
    CHECK(j.contains("recommendations"));
    CHECK(j.contains("should_skip"));
}

TEST_CASE("CLI predict skip file produces correct JSON", "[cli-predict]") {
    REQUIRE_CLI();
    TmpFile f{writeTmpJpeg("cli_skip.jpg")};

    const std::string cmd =
        std::string(MOSQUEEZE_CLI_PATH) + " predict \"" +
        f.path.string() + "\" --format json" + devNull();
    auto [output, rc] = runCmd(cmd);

    CHECK(rc == 0);
    const auto j = nlohmann::json::parse(output, nullptr, false);
    CHECK_FALSE(j.is_discarded());
    CHECK(j.value("should_skip", false) == true);
    CHECK(j.contains("skip_reason"));
}

TEST_CASE("CLI predict --prefer-speed changes output", "[cli-predict]") {
    REQUIRE_CLI();
    TmpFile f{writeTmp("cli3.json", std::string(1000, '{'))};

    const std::string cmdNormal =
        std::string(MOSQUEEZE_CLI_PATH) + " predict \"" +
        f.path.string() + "\" --format json" + devNull();
    const std::string cmdSpeed =
        std::string(MOSQUEEZE_CLI_PATH) + " predict \"" +
        f.path.string() + "\" --format json --prefer-speed" + devNull();

    auto [outNormal, rcN] = runCmd(cmdNormal);
    auto [outSpeed,  rcS] = runCmd(cmdSpeed);

    CHECK(rcN == 0);
    CHECK(rcS == 0);

    const auto jN = nlohmann::json::parse(outNormal, nullptr, false);
    const auto jS = nlohmann::json::parse(outSpeed,  nullptr, false);
    REQUIRE_FALSE(jN.is_discarded());
    REQUIRE_FALSE(jS.is_discarded());

    // Both must have recommendations; speed-first may reorder them
    REQUIRE(jN.contains("recommendations"));
    REQUIRE(jS.contains("recommendations"));
}

TEST_CASE("CLI predict reports error for missing file", "[cli-predict]") {
    REQUIRE_CLI();
    const std::string cmd =
        std::string(MOSQUEEZE_CLI_PATH) +
        " predict /no/such/file/xyz.json" + devNull();
    auto [output, rc] = runCmd(cmd);
    CHECK(rc != 0);
}

TEST_CASE("CLI predict batch: multiple files in one call", "[cli-predict]") {
    REQUIRE_CLI();
    TmpFile f1{writeTmp("batch1.json", R"({"a":1})")};
    TmpFile f2{writeTmp("batch2.cpp",  "int main(){}")};

    const std::string cmd =
        std::string(MOSQUEEZE_CLI_PATH) + " predict \"" +
        f1.path.string() + "\" \"" + f2.path.string() +
        "\" --format json" + devNull();
    auto [output, rc] = runCmd(cmd);

    CHECK(rc == 0);
    // Multi-file → JSON array
    const auto j = nlohmann::json::parse(output, nullptr, false);
    REQUIRE_FALSE(j.is_discarded());
    REQUIRE(j.is_array());
    CHECK(j.size() == 2);
}

TEST_CASE("CLI predict writes to output file with -o", "[cli-predict]") {
    REQUIRE_CLI();
    TmpFile input{writeTmp("cli_out.json", R"({"x":1})")};
    const fs::path outPath =
        fs::temp_directory_path() / "cli_int_test_out.json";
    TmpFile output{outPath};

    const std::string cmd =
        std::string(MOSQUEEZE_CLI_PATH) + " predict \"" +
        input.path.string() + "\" --format json -o \"" +
        outPath.string() + "\"" + devNull();
    auto [stdout_txt, rc] = runCmd(cmd);

    CHECK(rc == 0);
    // stdout should be empty (written to file)
    CHECK(stdout_txt.find("recommendations") == std::string::npos);
    // file should exist and contain JSON
    std::error_code ec;
    CHECK(fs::exists(outPath, ec));
    std::ifstream f(outPath);
    std::string content((std::istreambuf_iterator<char>(f)), {});
    const auto j = nlohmann::json::parse(content, nullptr, false);
    CHECK_FALSE(j.is_discarded());
}
