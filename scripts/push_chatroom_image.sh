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
LOCAL_CHATROOM="myself-chatroom-distribute-chatroom-app:latest"

# 远程仓库ACR路径
REMOTE_CHATROOM_LATEST="$REG/$NS/my_project-chatroom-app:latest"
REMOTE_CHATROOM_VERSION="$REG/$NS/my_project-chatroom-app:$DATE_TAG"

echo "Tagging chatroom-app image..."
docker tag "$LOCAL_CHATROOM" "$REMOTE_CHATROOM_LATEST"
docker tag "$LOCAL_CHATROOM" "$REMOTE_CHATROOM_VERSION"

echo "Pushing chatroom-app image..."
docker push "$REMOTE_CHATROOM_LATEST"
docker push "$REMOTE_CHATROOM_VERSION"

echo
echo "Done, chatroom-app image pushed to $REG successfully."
echo "  - $REMOTE_CHATROOM_LATEST"
echo "  - $REMOTE_CHATROOM_VERSION"
