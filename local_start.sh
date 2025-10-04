#!/bin/bash

# Docker 镜像构建脚本
set -e

echo "开始构建 Docker 镜像..."

# 使用当前目录的 Dockerfile 构建镜像
docker build -t chatroom-server:v1.0 .

echo "Docker 镜像构建完成！"

# 显示镜像信息
echo "镜像信息："
docker images | grep chatroom-server

echo ""
echo "运行以下命令启动容器（推荐使用docker-compose）："
echo "docker-compose up -d"
echo ""
echo "或者单独运行容器（需要先启动依赖服务）："
echo "docker run -d \\"
echo "  -p 8080:8080 \\"
echo "  -p 8090:8090 \\"
echo "  -p 50051:50051 \\"
echo "  --name chatroom-server \\"
echo "  --link kafka:kafka \\"
echo "  --link redis:redis \\"
echo "  --link mysql:mysql \\"
echo "  -e KAFKA_BROKER=kafka:9092 \\"
echo "  -e REDIS_ADDR=redis:6379 \\"
echo "  -e MYSQL_HOST=mysql \\"
echo "  -e MYSQL_USER=root \\"
echo "  -e MYSQL_PASS=123 \\"
echo "  chatroom-server:v1.0"