cmake_minimum_required(VERSION 3.16)
project(job LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 添加 muduo 子目录
add_subdirectory(muduo/base)
add_subdirectory(muduo/net)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})
INCLUDE_DIRECTORIES(${PROTOBUF_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/service)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/proto)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/base)
INCLUDE_DIRECTORIES(/usr/include/jsoncpp)

AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/service SERVICE_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/proto PROTO_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/base BASE_LIST)

ADD_EXECUTABLE(job main.cc ${SERVICE_LIST} ${PROTO_LIST} ${BASE_LIST})

# 链接库
# Using imported targets (like gRPC::grpc++) correctly brings in all necessary transitive dependencies.
TARGET_LINK_LIBRARIES(job
    muduo_net
    muduo_base
    jsoncpp
    mysqlclient
    uuid
    rdkafka
    rdkafka++
    pthread
    gRPC::grpc++
    protobuf::libprotobuf
)

# 复制配置文件到输出目录
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/job.conf
              ${CMAKE_BINARY_DIR}/bin/job.conf
              COPYONLY)