REG=crpi-hoas2aqx96meuhtw.cn-shenzhen.personal.cr.aliyuncs.com
NS=utopianyouth

docker tag myself-chatroom-distribute-logic-app:latest $REG/$NS/my_project-logic-app:v1.0
docker tag myself-chatroom-distribute-job-app:latest $REG/$NS/my_project-job-app:v1.0
docker tag myself-chatroom-distribute-chatroom-app:latest $REG/$NS/my_project-chatroom-app:v1.0
docker tag myself-chatroom-distribute-web-frontend:latest $REG/$NS/my_project-web-frontend:v1.0

docker push $REG/$NS/my_project-logic-app:v1.0
docker push $REG/$NS/my_project-job-app:v1.0
docker push $REG/$NS/my_project-chatroom-app:v1.0
docker push $REG/$NS/my_project-web-frontend:v1.0


docker tag bitnami/zookeeper:3.9.2 $REG/$NS/my_project-zookeeper:3.9.2
docker tag bitnami/kafka:3.7 $REG/$NS/my_project-kafka:3.7
docker tag redis:6.0.16 $REG/$NS/my_project-redis:6.0.16
docker tag mysql:8.0.37 $REG/$NS/my_project-mysql:8.0.37

docker push $REG/$NS/my_project-zookeeper:3.9.2
docker push $REG/$NS/my_project-kafka:3.7
docker push $REG/$NS/my_project-redis:6.0.16
docker push $REG/$NS/my_project-mysql:8.0.37