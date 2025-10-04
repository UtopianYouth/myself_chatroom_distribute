cmake_minimum_required(VERSION 3.10)

# 项目名称和版本
project(logic VERSION 1.0.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# 包含目录设置
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})  # 包含项目根目录
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})  # 包含当前目录
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/api)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/base)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/mysql)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/proto)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/redis)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/service)

# 包含系统库目录
INCLUDE_DIRECTORIES(/usr/include/mysql)
INCLUDE_DIRECTORIES(/usr/include/jsoncpp)
INCLUDE_DIRECTORIES(/usr/include/librdkafka)

# 收集源文件
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/api API_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/base BASE_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/mysql MYSQL_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/redis REDIS_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/service SERVICE_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/proto PROTO_LIST)

# 打印收集到的源文件（调试用）
message(STATUS "API_LIST: ${API_LIST}")
message(STATUS "SERVICE_LIST: ${SERVICE_LIST}")
message(STATUS "BASE_LIST: ${BASE_LIST}")
message(STATUS "MYSQL_LIST: ${MYSQL_LIST}")
message(STATUS "REDIS_LIST: ${REDIS_LIST}")
message(STATUS "PROTO_LIST: ${PROTO_LIST}")

# 添加可执行文件
ADD_EXECUTABLE(logic
    main.cc
    ${API_LIST}
    ${BASE_LIST}
    ${MYSQL_LIST}
    ${REDIS_LIST}
    ${SERVICE_LIST}
    ${PROTO_LIST}
)

# Find Protobuf and gRPC
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")
message(STATUS "Using gRPC ${gRPC_VERSION}")

# 链接库
TARGET_LINK_LIBRARIES(logic
    muduo_net
    muduo_base
    jsoncpp
    mysqlclient
    rdkafka
    rdkafka++
    pthread
    uuid
    ssl
    crypto
    curl
    gRPC::grpc++
    protobuf::libprotobuf
)

# 设置输出路径
set_target_properties(logic PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 复制配置文件到输出目录
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/logic.conf
              ${CMAKE_BINARY_DIR}/bin/logic.conf
              COPYONLY)