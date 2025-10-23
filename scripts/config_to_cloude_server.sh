#!/usr/bin/env bash
set -euo pipefail

# 交互式输入（密码密文）
read -p "ECS IP: " SSH_HOST
read -p "Username: " SSH_USER
read -s -p "Password: " SSH_PASSWORD
echo
read -p "Remote project root (e.g. /home/utopianyouth/myself-chatroom-distribute): " REMOTE_DIR

# 解析仓库根目录（脚本所在目录的上一级）
SCRIPT_DIR="$(cd -- "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# 本地源路径
DEPLOY_DIR="$REPO_ROOT/deploy"
NGINX_DIR="$REPO_ROOT/nginx"
SQL_FILE="$REPO_ROOT/server/create_tables.sql"
ENV_FILE="$REPO_ROOT/deploy/.env"

# 远端准备目录
sshpass -p "$SSH_PASSWORD" ssh -o StrictHostKeyChecking=no "$SSH_USER@$SSH_HOST" "mkdir -p '$REMOTE_DIR' '$REMOTE_DIR/deploy' '$REMOTE_DIR/nginx' '$REMOTE_DIR/server'"

# 同步 deploy/（目录）与 nginx/（目录）
sshpass -p "$SSH_PASSWORD" scp -o StrictHostKeyChecking=no -r "$DEPLOY_DIR" "$SSH_USER@$SSH_HOST:$REMOTE_DIR/"
sshpass -p "$SSH_PASSWORD" scp -o StrictHostKeyChecking=no -r "$NGINX_DIR" "$SSH_USER@$SSH_HOST:$REMOTE_DIR/"

# 同步 deploy/.env
if [ -f "$ENV_FILE" ]; then
  sshpass -p "$SSH_PASSWORD" scp -o StrictHostKeyChecking=no "$ENV_FILE" "$SSH_USER@$SSH_HOST:$REMOTE_DIR/deploy/.env"
fi

# 同步 server/create_tables.sql
if [ -f "$SQL_FILE" ]; then
  sshpass -p "$SSH_PASSWORD" scp -o StrictHostKeyChecking=no "$SQL_FILE" "$SSH_USER@$SSH_HOST:$REMOTE_DIR/server/"
fi

echo "Configuration sync done."