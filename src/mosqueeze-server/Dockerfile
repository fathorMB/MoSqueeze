FROM debian:bookworm AS vcpkg-base

# Install build tools - cached layer
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    curl \
    zip \
    unzip \
    tar \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Setup vcpkg
ENV VCPKG_ROOT=/opt/vcpkg
ENV VCPKG_BINARY_SOURCES=clear

RUN git clone https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics

WORKDIR /app

# Copy ONLY dependency manifests first (maximizes Docker layer cache)
COPY vcpkg.json vcpkg-configuration.json* ./

# Install dependencies in manifest mode (does not require CMakeLists.txt in /app)
RUN ${VCPKG_ROOT}/vcpkg install --clean-after-build \
    && echo "Dependencies ready"

FROM vcpkg-base AS build

WORKDIR /app

# Copy source code (separate layer - only invalidates on code change)
COPY CMakeLists.txt ./
COPY src/ ./src/
COPY cmake/ ./cmake/
COPY tests/ ./tests/
COPY third_party/ ./third_party/

# Build application
RUN cmake -B build -S . \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -DMOSQUEEZE_BUILD_SERVER=ON \
    -DMOSQUEEZE_BUILD_TESTS=OFF \
    && cmake --build build --config Release --target mosqueeze-server

FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --shell /bin/bash mosqueeze

USER mosqueeze
WORKDIR /home/mosqueeze

COPY --from=build --chown=mosqueeze:mosqueeze /app/build/src/mosqueeze-server/mosqueeze-server /usr/local/bin/mosqueeze-server

EXPOSE 50051

ENV MOSQUEEZE_HOST=0.0.0.0
ENV MOSQUEEZE_PORT=50051
ENV MOSQUEEZE_LOG_LEVEL=info

ENTRYPOINT ["/usr/local/bin/mosqueeze-server"]
