cmake_minimum_required(VERSION 3.16)
project(logic LANGUAGES C CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加 muduo 子目录
add_subdirectory(muduo/base)
add_subdirectory(muduo/net)

# 查找依赖包
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

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
INCLUDE_DIRECTORIES(${PROTOBUF_INCLUDE_DIRS})

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

# 链接库
# Using imported targets (like gRPC::grpc++) correctly brings in all necessary transitive dependencies.
TARGET_LINK_LIBRARIES(logic
    muduo_net
    muduo_base
    jsoncpp
    mysqlclient
    rdkafka
    rdkafka++
    pthread
    uuid
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