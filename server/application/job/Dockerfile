# Stage 1: Builder
FROM ubuntu:20.04 AS builder
WORKDIR /app

RUN sed -i 's/archive.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list && \
    sed -i 's/security.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list && \
    apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake libmysqlclient-dev libboost-all-dev pkg-config libjsoncpp-dev \
    librdkafka-dev libcurl4-openssl-dev uuid-dev autoconf automake libtool curl git unzip \
    ca-certificates openssl && update-ca-certificates && rm -rf /var/lib/apt/lists/*

# Build Protobuf
RUN set -eux; \
    PROTO_VERSION=3.21.12; \
    curl -L --retry 5 "https://ghproxy.com/https://github.com/protocolbuffers/protobuf/releases/download/v21.12/protobuf-cpp-${PROTO_VERSION}.tar.gz" -o /tmp/protobuf.tar.gz || \
    curl -L --retry 5 "https://github.com/protocolbuffers/protobuf/releases/download/v21.12/protobuf-cpp-${PROTO_VERSION}.tar.gz" -o /tmp/protobuf.tar.gz; \
    tar -xf /tmp/protobuf.tar.gz -C /tmp; \
    PROTO_DIR=$(tar -tf /tmp/protobuf.tar.gz | head -1 | cut -d/ -f1); \
    cd /tmp/${PROTO_DIR}; ./configure; make -j"$(nproc)"; make install; ldconfig; \
    rm -rf /tmp/protobuf.tar.gz /tmp/${PROTO_DIR}

# Copy repo
COPY . .

# Build and install gRPC
RUN set -eux; \
    mv /app/grpc /tmp/grpc; \
    rm -rf /tmp/grpc/cmake/build; mkdir -p /tmp/grpc/cmake/build; \
    cd /tmp/grpc/cmake/build; \
    cmake ../.. -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=/usr/local; \
    make -j"$(nproc)"; make install; ldconfig; rm -rf /tmp/grpc

ENV PATH="/usr/local/bin:${PATH}"
ENV PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:${PKG_CONFIG_PATH}"
ENV LD_LIBRARY_PATH="/usr/local/lib:${LD_LIBRARY_PATH}"

# Regenerate job protos
RUN protoc -I=/app/application/job/proto \
    --cpp_out=/app/application/job/proto \
    --grpc_out=/app/application/job/proto \
    --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) \
    /app/application/job/proto/*.proto

# Configure & build only job target
RUN cmake -B /app/build -S /app/application/job \
      -DProtobuf_DIR=/usr/local/lib/cmake/protobuf \
      -DgRPC_DIR=/usr/local/lib/cmake/grpc && \
    cmake --build /app/build --target job --parallel $(nproc)

# Stage 2: Runtime
FROM ubuntu:20.04
WORKDIR /app

RUN sed -i 's/archive.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list && \
    sed -i 's/security.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list && \
    apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    libmysqlclient21 libboost-program-options1.71.0 libboost-system1.71.0 libboost-filesystem1.71.0 \
    libjsoncpp1 librdkafka++1 libcurl4 libuuid1 && rm -rf /var/lib/apt/lists/*

COPY --from=builder /usr/local/lib/libprotobuf* /usr/local/lib/
COPY --from=builder /usr/local/lib/libgrpc* /usr/local/lib/
RUN ldconfig

COPY --from=builder /app/build/bin/job /app/bin/job
COPY --from=builder /app/application/job/job.conf /app/job.conf

CMD ["/app/bin/job"]