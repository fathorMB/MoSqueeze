#pragma once

#include "compression.grpc.pb.h"
#include "compressor.hpp"

#include <grpcpp/grpcpp.h>

#include <memory>

namespace mosqueeze::server {

class CompressionServiceImpl final : public Compression::Service {
public:
    explicit CompressionServiceImpl(std::shared_ptr<Compressor> compressor);

    grpc::Status Compress(
        grpc::ServerContext* context,
        const CompressRequest* request,
        CompressResponse* response) override;

    grpc::Status Decompress(
        grpc::ServerContext* context,
        const DecompressRequest* request,
        DecompressResponse* response) override;

    grpc::Status StreamCompress(
        grpc::ServerContext* context,
        grpc::ServerReaderWriter<StreamCompressResponse, StreamCompressRequest>* stream) override;

    grpc::Status StreamDecompress(
        grpc::ServerContext* context,
        grpc::ServerReaderWriter<StreamDecompressResponse, StreamDecompressRequest>* stream) override;

    grpc::Status GetAlgorithms(
        grpc::ServerContext* context,
        const GetAlgorithmsRequest* request,
        GetAlgorithmsResponse* response) override;

    grpc::Status HealthCheck(
        grpc::ServerContext* context,
        const HealthCheckRequest* request,
        HealthCheckResponse* response) override;

private:
    std::shared_ptr<Compressor> compressor_;
};

} // namespace mosqueeze::server
