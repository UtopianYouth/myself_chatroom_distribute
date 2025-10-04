#!/bin/bash
set -e

# 基础服务地址（compose 中通过环境变量注入；默认指向同网络容器名）
export KAFKA_BROKER="${KAFKA_BROKER:-kafka:9092}"
export ZK_ADDR="${ZK_ADDR:-zookeeper:2181}"
export REDIS_ADDR="${REDIS_ADDR:-redis:6379}"
export MYSQL_HOST="${MYSQL_HOST:-mysql}"
export MYSQL_PORT="${MYSQL_PORT:-3306}"
export MYSQL_USER="${MYSQL_USER:-root}"
export MYSQL_PASS="${MYSQL_PASS:-123}"

# 如有需要，导出到应用可读取的配置文件或环境
echo "Starting chat-room, job, logic with:"
echo "KAFKA_BROKER=$KAFKA_BROKER"
echo "ZK_ADDR=$ZK_ADDR"
echo "REDIS_ADDR=$REDIS_ADDR"
echo "MYSQL_HOST=$MYSQL_HOST:$MYSQL_PORT"

# 启动顺序: logic, job, chat-room
echo "Starting logic server in background..."
/app/bin/logic &

echo "Waiting for logic server to initialize..."
sleep 5 # 等待5秒，让logic有时间启动

echo "Starting job server in background..."
/app/bin/job &

echo "Starting chat-room server in foreground..."
exec /app/bin/chat-room