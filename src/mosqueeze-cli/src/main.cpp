#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <mosqueeze/Version.hpp>
#include <spdlog/spdlog.h>

#include <cstdint>

int main(int argc, char** argv) {
    CLI::App app{"MoSqueeze - Cold Storage Compression Library"};

    // Version flag
    app.add_flag("-V,--version", [](std::int64_t) {
        fmt::print("mosqueeze v{}\n", mosqueeze::versionString());
        throw CLI::Success();
    }, "Print version and exit");

    // For now, subcommands will be added in later issues
    // Just basic parse for scaffold verification

    CLI11_PARSE(app, argc, argv);

    return 0;
}
