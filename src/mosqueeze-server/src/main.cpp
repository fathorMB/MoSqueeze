#include "service.hpp"
#include "compressor.hpp"

#include <grpcpp/grpcpp.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

std::string getenvOr(const char* key, const char* fallback) {
    const char* value = std::getenv(key);
    return value != nullptr ? std::string(value) : std::string(fallback);
}

int getenvIntOr(const char* key, int fallback) {
    const char* value = std::getenv(key);
    if (value == nullptr) {
        return fallback;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    const std::string host = getenvOr("MOSQUEEZE_HOST", "0.0.0.0");
    const int port = getenvIntOr("MOSQUEEZE_PORT", 50051);
    const std::string logLevel = getenvOr("MOSQUEEZE_LOG_LEVEL", "info");

    if (logLevel == "trace") {
        spdlog::set_level(spdlog::level::trace);
    } else if (logLevel == "debug") {
        spdlog::set_level(spdlog::level::debug);
    } else if (logLevel == "warn") {
        spdlog::set_level(spdlog::level::warn);
    } else if (logLevel == "error") {
        spdlog::set_level(spdlog::level::err);
    } else {
        spdlog::set_level(spdlog::level::info);
    }

    const std::string address = host + ":" + std::to_string(port);
    auto compressor = std::make_shared<mosqueeze::server::Compressor>();
    mosqueeze::server::CompressionServiceImpl service(compressor);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        spdlog::error("Failed to start MoSqueeze gRPC server on {}", address);
        return 1;
    }

    spdlog::info("MoSqueeze gRPC server listening on {}", address);
    server->Wait();
    return 0;
}
