FROM debian:bookworm AS build

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    curl \
    zip \
    unzip \
    tar \
    && rm -rf /var/lib/apt/lists/*

ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics

WORKDIR /app
COPY . .

RUN cmake -B build -S . \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -DMOSQUEEZE_BUILD_SERVER=ON \
    -DMOSQUEEZE_BUILD_TESTS=OFF \
    && cmake --build build --config Release --target mosqueeze-server

FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /app/build/src/mosqueeze-server/mosqueeze-server /usr/local/bin/mosqueeze-server

EXPOSE 50051

ENV MOSQUEEZE_HOST=0.0.0.0
ENV MOSQUEEZE_PORT=50051
ENV MOSQUEEZE_LOG_LEVEL=info

ENTRYPOINT ["/usr/local/bin/mosqueeze-server"]
