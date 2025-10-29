#!/usr/bin/env bash
set -euo pipefail

# 执行脚本前先登录到远程仓库

# ACR地址与命名空间
REG=${REG:-crpi-hoas2aqx96meuhtw.cn-shenzhen.personal.cr.aliyuncs.com}
NS=${NS:-utopianyouth}

# 时间戳标签（便于回滚）
DATE_TAG=$(date +%Y%m%d-%H%M%S)

echo "Registry: $REG"
echo "Namespace: $NS"
echo "Version tag: $DATE_TAG"
echo

# 本地已构建的业务镜像名称
LOCAL_WEB="myself-chatroom-distribute-web-frontend:latest"

# 远程仓库ACR路径
REMOTE_WEB_LATEST="$REG/$NS/my_project-web-frontend:latest"
REMOTE_WEB_VERSION="$REG/$NS/my_project-web-frontend:$DATE_TAG"

echo "Tagging web-frontend image..."
docker tag "$LOCAL_WEB" "$REMOTE_WEB_LATEST"
docker tag "$LOCAL_WEB" "$REMOTE_WEB_VERSION"

echo "Pushing web-frontend image..."
docker push "$REMOTE_WEB_LATEST"
docker push "$REMOTE_WEB_VERSION"

echo
echo "Done, web-frontend image pushed to $REG successfully."
echo "  - $REMOTE_WEB_LATEST"
echo "  - $REMOTE_WEB_VERSION"
