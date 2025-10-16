cmake_minimum_required(VERSION 3.16)
project(chatroom LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 将可执行文件输出到 build/bin 目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 添加 muduo 子目录
add_subdirectory(muduo/base)
add_subdirectory(muduo/net)

# 设置 muduo 头文件包含路径
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/muduo)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/muduo/base)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/muduo/net)

# 查找依赖包
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# 设置库文件输出路径
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/base)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/api)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/service)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/rpc)
INCLUDE_DIRECTORIES(${PROTOBUF_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(/usr/include/jsoncpp)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/base BASE_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/api API_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/service SERVICE_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/rpc RPC_LIST)


ADD_EXECUTABLE(chat-room main.cc ${SERVICE_LIST} ${BASE_LIST} ${API_LIST} ${RPC_LIST})


# find_package calls are now in the top-level CMakeLists.txt and are no longer needed here.

# Using imported targets (like gRPC::grpc++) correctly brings in all necessary 
# transitive dependencies (like gpr, absl, ssl, crypto, zlib, etc.).
TARGET_LINK_LIBRARIES(chat-room muduo_net muduo_base jsoncpp curl
    gRPC::grpc++
    protobuf::libprotobuf
)

# 复制配置文件到输出目录
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/chat-room.conf
    ${CMAKE_BINARY_DIR}/bin/chat-room.conf
    COPYONLY
)