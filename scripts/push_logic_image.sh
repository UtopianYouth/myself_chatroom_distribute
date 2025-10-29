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
LOCAL_LOGIC="myself-chatroom-distribute-logic-app:latest"

# 远程仓库ACR路径
REMOTE_LOGIC_LATEST="$REG/$NS/my_project-logic-app:latest"
REMOTE_LOGIC_VERSION="$REG/$NS/my_project-logic-app:$DATE_TAG"

echo "Tagging logic-app image..."
docker tag "$LOCAL_LOGIC" "$REMOTE_LOGIC_LATEST"
docker tag "$LOCAL_LOGIC" "$REMOTE_LOGIC_VERSION"

echo "Pushing logic-app image..."
docker push "$REMOTE_LOGIC_LATEST"
docker push "$REMOTE_LOGIC_VERSION"

echo
echo "Done, logic-app image pushed to $REG successfully."
echo "  - $REMOTE_LOGIC_LATEST"
echo "  - $REMOTE_LOGIC_VERSION"
