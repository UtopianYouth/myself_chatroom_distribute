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
LOCAL_JOB="myself-chatroom-distribute-job-app:latest"

# 远程仓库ACR路径
REMOTE_JOB_LATEST="$REG/$NS/my_project-job-app:latest"
REMOTE_JOB_VERSION="$REG/$NS/my_project-job-app:$DATE_TAG"

echo "Tagging job-app image..."
docker tag "$LOCAL_JOB" "$REMOTE_JOB_LATEST"
docker tag "$LOCAL_JOB" "$REMOTE_JOB_VERSION"

echo "Pushing job-app image..."
docker push "$REMOTE_JOB_LATEST"
docker push "$REMOTE_JOB_VERSION"

echo
echo "Done, job-app image pushed to $REG successfully."
echo "  - $REMOTE_JOB_LATEST"
echo "  - $REMOTE_JOB_VERSION"
