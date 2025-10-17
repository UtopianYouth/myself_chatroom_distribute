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

# 本地已构建的业务镜像名称（确保存在）
LOCAL_LOGIC="myself-chatroom-distribute-logic-app:latest"
LOCAL_JOB="myself-chatroom-distribute-job-app:latest"
LOCAL_CHATROOM="myself-chatroom-distribute-chatroom-app:latest"
LOCAL_WEB="myself-chatroom-distribute-web-frontend:latest"

# 远程仓库ACR路径
REMOTE_LOGIC_LATEST="$REG/$NS/my_project-logic-app:latest"
REMOTE_LOGIC_VERSION="$REG/$NS/my_project-logic-app:$DATE_TAG"

REMOTE_JOB_LATEST="$REG/$NS/my_project-job-app:latest"
REMOTE_JOB_VERSION="$REG/$NS/my_project-job-app:$DATE_TAG"

REMOTE_CHATROOM_LATEST="$REG/$NS/my_project-chatroom-app:latest"
REMOTE_CHATROOM_VERSION="$REG/$NS/my_project-chatroom-app:$DATE_TAG"

REMOTE_WEB_LATEST="$REG/$NS/my_project-web-frontend:latest"
REMOTE_WEB_VERSION="$REG/$NS/my_project-web-frontend:$DATE_TAG"

echo "Tagging business images..."
docker tag "$LOCAL_LOGIC" "$REMOTE_LOGIC_LATEST"
docker tag "$LOCAL_LOGIC" "$REMOTE_LOGIC_VERSION"

docker tag "$LOCAL_JOB" "$REMOTE_JOB_LATEST"
docker tag "$LOCAL_JOB" "$REMOTE_JOB_VERSION"

docker tag "$LOCAL_CHATROOM" "$REMOTE_CHATROOM_LATEST"
docker tag "$LOCAL_CHATROOM" "$REMOTE_CHATROOM_VERSION"

docker tag "$LOCAL_WEB" "$REMOTE_WEB_LATEST"
docker tag "$LOCAL_WEB" "$REMOTE_WEB_VERSION"

echo "Pushing business images..."
docker push "$REMOTE_LOGIC_LATEST"
docker push "$REMOTE_LOGIC_VERSION"

docker push "$REMOTE_JOB_LATEST"
docker push "$REMOTE_JOB_VERSION"

docker push "$REMOTE_CHATROOM_LATEST"
docker push "$REMOTE_CHATROOM_VERSION"

docker push "$REMOTE_WEB_LATEST"
docker push "$REMOTE_WEB_VERSION"

echo
echo "Done, all project images pushed to $REG successfully."