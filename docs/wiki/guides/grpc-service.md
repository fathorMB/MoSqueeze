# gRPC Service (`mosqueeze-server`)

**Summary**: Optional gRPC microservice exposing MoSqueeze compression/decompression APIs on port `50051`.

**Last updated**: 2026-04-23

---

## Build

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DMOSQUEEZE_BUILD_SERVER=ON
cmake --build build --config Release --target mosqueeze-server
```

On Windows, the first configure after enabling `MOSQUEEZE_BUILD_SERVER` can take several minutes because `vcpkg` compiles `grpc` and `protobuf` (plus transitive dependencies like `abseil` and `re2`).

---

## Run

```bash
./build/src/mosqueeze-server/mosqueeze-server
```

Windows (Visual Studio multi-config):

```powershell
.\build\src\mosqueeze-server\Release\mosqueeze-server.exe
```

Defaults:

- host: `0.0.0.0`
- port: `50051`
- log level: `info`

Environment overrides:

- `MOSQUEEZE_HOST`
- `MOSQUEEZE_PORT`
- `MOSQUEEZE_LOG_LEVEL`

---

## RPCs

- `Compress`
- `Decompress`
- `StreamCompress`
- `StreamDecompress`
- `GetAlgorithms`
- `HealthCheck`

Protocol definition:

- `src/mosqueeze-server/proto/compression.proto`

---

## Notes

- Server build is optional and disabled by default (`MOSQUEEZE_BUILD_SERVER=OFF`).
- The service uses `libmosqueeze` compression pipeline and preprocessors.
- Build dependencies added in `vcpkg.json`: `grpc`, `protobuf`.
