#include "service.hpp"

#include <mosqueeze/Version.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace mosqueeze::server {

CompressionServiceImpl::CompressionServiceImpl(std::shared_ptr<Compressor> compressor)
    : compressor_(std::move(compressor)) {}

grpc::Status CompressionServiceImpl::Compress(
    grpc::ServerContext* context,
    const CompressRequest* request,
    CompressResponse* response) {
    (void)context;
    if (request == nullptr || response == nullptr) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Null request/response");
    }

    const std::vector<uint8_t> input(request->payload().begin(), request->payload().end());
    const auto result = compressor_->compress(
        input,
        request->algorithm().empty() ? "zstd" : request->algorithm(),
        request->level(),
        request->preprocessor().empty() ? "none" : request->preprocessor());

    response->set_success(result.success);
    response->set_input_size(result.inputSize);
    response->set_output_size(result.outputSize);
    response->set_ratio(result.ratio);
    response->set_algorithm(result.algorithm);
    response->set_level(result.level);
    response->set_preprocessor_used(result.preprocessorUsed);
    response->set_error_message(result.errorMessage);
    if (result.success) {
        response->set_payload(std::string(result.output.begin(), result.output.end()));
    }

    return grpc::Status::OK;
}

grpc::Status CompressionServiceImpl::Decompress(
    grpc::ServerContext* context,
    const DecompressRequest* request,
    DecompressResponse* response) {
    (void)context;
    if (request == nullptr || response == nullptr) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Null request/response");
    }

    const std::vector<uint8_t> input(request->payload().begin(), request->payload().end());
    const auto result = compressor_->decompress(input);

    response->set_success(result.success);
    response->set_input_size(result.inputSize);
    response->set_output_size(result.outputSize);
    response->set_preprocessor_used(result.preprocessorUsed);
    response->set_error_message(result.errorMessage);
    if (result.success) {
        response->set_payload(std::string(result.output.begin(), result.output.end()));
    }

    return grpc::Status::OK;
}

grpc::Status CompressionServiceImpl::StreamCompress(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<StreamCompressResponse, StreamCompressRequest>* stream) {
    (void)context;
    if (stream == nullptr) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Null stream");
    }

    std::string algorithm = "zstd";
    int32_t level = 0;
    std::string preprocessor = "none";
    int64_t expectedSize = 0;
    std::vector<uint8_t> payload;

    StreamCompressRequest request;
    while (stream->Read(&request)) {
        if (request.has_metadata()) {
            algorithm = request.metadata().algorithm().empty() ? "zstd" : request.metadata().algorithm();
            level = request.metadata().level();
            preprocessor = request.metadata().preprocessor().empty() ? "none" : request.metadata().preprocessor();
            expectedSize = request.metadata().expected_size();
            continue;
        }
        if (request.has_chunk()) {
            const std::string& chunk = request.chunk();
            payload.insert(payload.end(), chunk.begin(), chunk.end());

            StreamCompressResponse progressResp;
            auto* progress = progressResp.mutable_progress();
            progress->set_bytes_processed(static_cast<int64_t>(payload.size()));
            progress->set_total_bytes(expectedSize);
            progress->set_percentage(expectedSize > 0
                ? (100.0 * static_cast<double>(payload.size()) / static_cast<double>(expectedSize))
                : 0.0);
            stream->Write(progressResp);
        }
    }

    const auto result = compressor_->compress(payload, algorithm, level, preprocessor);
    StreamCompressResponse finalResp;
    auto* resp = finalResp.mutable_result();
    resp->set_success(result.success);
    resp->set_input_size(result.inputSize);
    resp->set_output_size(result.outputSize);
    resp->set_ratio(result.ratio);
    resp->set_algorithm(result.algorithm);
    resp->set_level(result.level);
    resp->set_preprocessor_used(result.preprocessorUsed);
    resp->set_error_message(result.errorMessage);
    if (result.success) {
        resp->set_payload(std::string(result.output.begin(), result.output.end()));
    }
    stream->Write(finalResp);
    return grpc::Status::OK;
}

grpc::Status CompressionServiceImpl::StreamDecompress(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<StreamDecompressResponse, StreamDecompressRequest>* stream) {
    (void)context;
    if (stream == nullptr) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Null stream");
    }

    std::vector<uint8_t> payload;
    StreamDecompressRequest request;
    while (stream->Read(&request)) {
        if (request.has_chunk()) {
            const std::string& chunk = request.chunk();
            payload.insert(payload.end(), chunk.begin(), chunk.end());

            StreamDecompressResponse progressResp;
            auto* progress = progressResp.mutable_progress();
            progress->set_bytes_processed(static_cast<int64_t>(payload.size()));
            progress->set_total_bytes(0);
            progress->set_percentage(0.0);
            stream->Write(progressResp);
        }
    }

    const auto result = compressor_->decompress(payload);
    StreamDecompressResponse finalResp;
    auto* resp = finalResp.mutable_result();
    resp->set_success(result.success);
    resp->set_input_size(result.inputSize);
    resp->set_output_size(result.outputSize);
    resp->set_preprocessor_used(result.preprocessorUsed);
    resp->set_error_message(result.errorMessage);
    if (result.success) {
        resp->set_payload(std::string(result.output.begin(), result.output.end()));
    }
    stream->Write(finalResp);
    return grpc::Status::OK;
}

grpc::Status CompressionServiceImpl::GetAlgorithms(
    grpc::ServerContext* context,
    const GetAlgorithmsRequest* request,
    GetAlgorithmsResponse* response) {
    (void)context;
    (void)request;
    if (response == nullptr) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Null response");
    }

    const std::vector<std::string> preprocessors = {
        "none", "json-canonical", "xml-canonical", "image-meta-strip", "png-optimizer", "bayer-raw"};

    for (const auto& algo : Compressor::supportedAlgorithms()) {
        auto* info = response->add_algorithms();
        info->set_name(algo);
        if (algo == "zstd") {
            info->set_display_name("Zstandard");
            info->set_description("Fast and versatile compression");
        } else if (algo == "brotli") {
            info->set_display_name("Brotli");
            info->set_description("Strong text compression");
        } else if (algo == "lzma") {
            info->set_display_name("LZMA");
            info->set_description("High-ratio compression");
        } else if (algo == "zpaq") {
            info->set_display_name("ZPAQ");
            info->set_description("Maximum ratio archival compression");
        }
        const auto [minLevel, maxLevel] = Compressor::algorithmLevelRange(algo);
        info->set_min_level(minLevel);
        info->set_max_level(maxLevel);
        info->set_default_level(Compressor::defaultLevel(algo));
        for (const auto& prep : preprocessors) {
            info->add_supported_preprocessors(prep);
        }
    }
    return grpc::Status::OK;
}

grpc::Status CompressionServiceImpl::HealthCheck(
    grpc::ServerContext* context,
    const HealthCheckRequest* request,
    HealthCheckResponse* response) {
    (void)context;
    (void)request;
    if (response == nullptr) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Null response");
    }

    response->set_status(HealthCheckResponse::SERVING);
    response->set_version(mosqueeze::versionString());
    (*response->mutable_details())["service"] = "mosqueeze.Compression";
    (*response->mutable_details())["status"] = "ok";
    return grpc::Status::OK;
}

} // namespace mosqueeze::server
