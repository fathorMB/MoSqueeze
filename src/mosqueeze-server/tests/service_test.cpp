#include "service.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

int main() {
    auto compressor = std::make_shared<mosqueeze::server::Compressor>();
    mosqueeze::server::CompressionServiceImpl service(compressor);

    mosqueeze::CompressRequest compressReq;
    compressReq.set_payload("Hello MoSqueeze gRPC test payload");
    compressReq.set_algorithm("zstd");
    compressReq.set_level(3);
    compressReq.set_preprocessor("none");

    mosqueeze::CompressResponse compressResp;
    grpc::ServerContext compressCtx;
    auto status = service.Compress(&compressCtx, &compressReq, &compressResp);
    assert(status.ok());
    assert(compressResp.success());
    assert(!compressResp.payload().empty());

    mosqueeze::DecompressRequest decompReq;
    decompReq.set_payload(compressResp.payload());
    mosqueeze::DecompressResponse decompResp;
    grpc::ServerContext decompCtx;
    status = service.Decompress(&decompCtx, &decompReq, &decompResp);
    assert(status.ok());
    assert(decompResp.success());
    assert(decompResp.payload() == compressReq.payload());

    mosqueeze::GetAlgorithmsRequest algReq;
    mosqueeze::GetAlgorithmsResponse algResp;
    grpc::ServerContext algCtx;
    status = service.GetAlgorithms(&algCtx, &algReq, &algResp);
    assert(status.ok());
    assert(algResp.algorithms_size() >= 4);

    mosqueeze::HealthCheckRequest healthReq;
    mosqueeze::HealthCheckResponse healthResp;
    grpc::ServerContext healthCtx;
    status = service.HealthCheck(&healthCtx, &healthReq, &healthResp);
    assert(status.ok());
    assert(healthResp.status() == mosqueeze::HealthCheckResponse::SERVING);

    std::cout << "[PASS] mosqueeze-server-test\n";
    return 0;
}
