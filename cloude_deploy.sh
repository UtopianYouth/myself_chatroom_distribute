#!/bin/bash

# 部署脚本 - 用于云服务器部署
set -e

echo "开始部署聊天室服务器..."

# 检查 Docker 是否安装
if ! command -v docker &> /dev/null; then
    echo "Docker 未安装，开始安装 Docker..."
    curl -fsSL https://get.docker.com -o get-docker.sh
    sh get-docker.sh
    usermod -aG docker $USER
    systemctl enable docker
    systemctl start docker
fi

# 检查 Docker Compose 是否安装
if ! command -v docker-compose &> /dev/null; then
    echo "Docker Compose 未安装，开始安装..."
    curl -L "https://github.com/docker/compose/releases/latest/download/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
    chmod +x /usr/local/bin/docker-compose
fi

# 创建必要的目录
mkdir -p config logs

# 构建并启动服务
echo "构建 Docker 镜像..."
docker-compose build

echo "启动服务..."
docker-compose up -d

echo "部署完成！"
echo "服务运行在："
echo "- HTTP 服务: http://服务器IP:8080"
echo "- gRPC 服务: 9090 端口"

echo ""
echo "查看服务状态：docker-compose ps"
echo "查看日志：docker-compose logs -f"
echo "停止服务：docker-compose down"