# Stage 1: Build Stage
FROM ubuntu:20.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# 使用清华源并解决证书问题
RUN apt-get update && apt-get install -y ca-certificates && \
    sed -i 's|http://.*archive.ubuntu.com|https://mirrors.tuna.tsinghua.edu.cn|g' /etc/apt/sources.list && \
    sed -i 's|http://.*security.ubuntu.com|https://mirrors.tuna.tsinghua.edu.cn|g' /etc/apt/sources.list && \
    update-ca-certificates && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    gcc-10 \
    g++-10 \
    git \
    libssl-dev \
    libcurl4-openssl-dev \
    libjsoncpp-dev \
    uuid-dev \
    libmysqlclient-dev \
    libprotobuf-dev \
    protobuf-compiler \
    && rm -rf /var/lib/apt/lists/*

# Build and install gRPC from local source
COPY grpc /grpc
RUN cd /grpc && \
    rm -rf cmake/build && \
    mkdir -p cmake/build && \
    cd cmake/build && \
    cmake ../.. \
        -DgRPC_INSTALL=ON \
        -DCMAKE_BUILD_TYPE=Release \
        -DgRPC_BUILD_TESTS=OFF \
        -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j"$(nproc)" && \
    make install && \
    cd / && \
    rm -rf /grpc

# Build and install hiredis from local source
COPY hiredis /hiredis
RUN cd /hiredis && \
    make -j"$(nproc)" && \
    make install && \
    cd / && \
    rm -rf /hiredis

# Build and install librdkafka from local source
COPY librdkafka /librdkafka
RUN cd /librdkafka && \
    ./configure --prefix=/usr/local && \
    make -C src && \
    make -C src-cpp && \
    make -C src install && \
    make -C src-cpp install && \
    cd / && \
    rm -rf /librdkafka

WORKDIR /app

# Copy server directory from parent directory
RUN ls -l /app
COPY . /app/server
RUN ls -lR /app/server


# Compile the project
WORKDIR /app/server/application
RUN mkdir -p build && cd build && \
    CC=/usr/bin/gcc-10 CXX=/usr/bin/g++-10 cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_RPC=ON .. && \
    make -j"$(nproc)"

# Stage 2: Runtime Stage
FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl1.1 \
    libcurl4 \
    libjsoncpp1 \
    libuuid1 \
    libmysqlclient21 \
    libprotobuf17 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create a non-root user and group
RUN groupadd -r appgroup && useradd -r -g appgroup appuser

# Create app directory and bin directory
RUN mkdir -p /app/bin

# Copy runtime libraries from builder stage (gRPC, librdkafka, etc.)
COPY --from=builder /usr/local/lib /usr/local/lib
RUN ldconfig

# Copy compiled binaries from the builder stage
COPY --from=builder /app/server/build/bin/chat-room /app/bin/
COPY --from=builder /app/server/build/bin/logic /app/bin/
COPY --from=builder /app/server/build/bin/job /app/bin/

# Copy start script
COPY start.sh /app/start.sh
RUN chmod +x /app/start.sh

# Set ownership
RUN chown -R appuser:appgroup /app

# Switch to non-root user
USER appuser

WORKDIR /app

# Expose necessary ports
EXPOSE 8080 8090 50051

# Run the application
CMD ["/app/start.sh"]