#!/usr/bin/env bash
set -euo pipefail

# 执行脚本前先登录远程仓库

# 中间件镜像推送
REG=${REG:-crpi-hoas2aqx96meuhtw.cn-shenzhen.personal.cr.aliyuncs.com}
NS=${NS:-utopianyouth}

# 固定版本（与 deploy/docker-compose.yml 保持一致）
ZK_VER="3.9.2"
KAFKA_VER="3.7"
REDIS_VER="6.0.16"
MYSQL_VER="8.0.37"

echo "Registry: $REG"
echo "Namespace: $NS"
echo "Push middleware images with fixed versions:"
echo "  zookeeper:$ZK_VER kafka:$KAFKA_VER redis:$REDIS_VER mysql:$MYSQL_VER"
echo

docker tag "bitnami/zookeeper:$ZK_VER" "$REG/$NS/my_project-zookeeper:$ZK_VER"
docker tag "bitnami/kafka:$KAFKA_VER" "$REG/$NS/my_project-kafka:$KAFKA_VER"
docker tag "redis:$REDIS_VER" "$REG/$NS/my_project-redis:$REDIS_VER"
docker tag "mysql:$MYSQL_VER" "$REG/$NS/my_project-mysql:$MYSQL_VER"

docker push "$REG/$NS/my_project-zookeeper:$ZK_VER"
docker push "$REG/$NS/my_project-kafka:$KAFKA_VER"
docker push "$REG/$NS/my_project-redis:$REDIS_VER"
docker push "$REG/$NS/my_project-mysql:$MYSQL_VER"

echo "Done. Ensure deploy/docker-compose.yml uses these versions or official images accordingly."