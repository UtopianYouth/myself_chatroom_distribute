# INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})
# INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/base)
# INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/api)
# INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/mysql)
# INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/redis)
# INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/service)

# INCLUDE_DIRECTORIES(/usr/include/mysql)
# INCLUDE_DIRECTORIES(/usr/include/jsoncpp)
# AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/base BASE_LIST)
# AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/api API_LIST)
# AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/mysql MYSQL_LIST)
# AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/redis REDIS_LIST)
# AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/service SERVICE_LIST)

# ADD_EXECUTABLE(chat-room main.cc  ${SERVICE_LIST} ${BASE_LIST} ${API_LIST} ${MYSQL_LIST} ${REDIS_LIST} )

# TARGET_LINK_LIBRARIES(chat-room muduo_net jsoncpp mysqlclient uuid ssl crypto)


INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})


INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/base)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/api)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/mysql)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/redis)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/service)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/rpc)

INCLUDE_DIRECTORIES(/usr/include/mysql)
INCLUDE_DIRECTORIES(/usr/include/jsoncpp)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/base BASE_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/api API_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/mysql MYSQL_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/redis REDIS_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/service SERVICE_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/rpc RPC_LIST)

# 根据RPC开关决定是否包含RPC源文件
if(ENABLE_RPC)
    ADD_EXECUTABLE(chat-room main.cc  ${SERVICE_LIST} 
        ${BASE_LIST} ${API_LIST} ${MYSQL_LIST}
        ${REDIS_LIST} ${RPC_LIST})
else()
    ADD_EXECUTABLE(chat-room main.cc  ${SERVICE_LIST} 
        ${BASE_LIST} ${API_LIST} ${MYSQL_LIST}
        ${REDIS_LIST})
endif()

# Find Protobuf installation
if(ENABLE_RPC)
    # Looks for protobuf-config.cmake file installed by Protobuf's cmake installation.
    set(protobuf_MODULE_COMPATIBLE TRUE)
    # 添加 gRPC 和 Protobuf 相关路径
    find_package(Protobuf REQUIRED)
    find_package(gRPC CONFIG REQUIRED)
    message(STATUS "Using protobuf ${Protobuf_VERSION}")
    
    set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
    set(_REFLECTION gRPC::grpc++_reflection)
    if(CMAKE_CROSSCOMPILING)
        find_program(_PROTOBUF_PROTOC protoc)
    else()
        set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
    endif()
    
    # Find gRPC installation
    # Looks for gRPCConfig.cmake file installed by gRPC's cmake installation.
    find_package(gRPC CONFIG REQUIRED)
    message(STATUS "Using gRPC ${gRPC_VERSION}")
    
    set(_GRPC_GRPCPP gRPC::grpc++)
    if(CMAKE_CROSSCOMPILING)
        find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
    else()
        set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
    endif()
endif()

# 根据RPC开关决定链接的库
if(ENABLE_RPC)
    TARGET_LINK_LIBRARIES(chat-room muduo_net jsoncpp 
        mysqlclient uuid ssl crypto
        ${_REFLECTION}
        ${_GRPC_GRPCPP}
        ${_PROTOBUF_LIBPROTOBUF})
else()
    TARGET_LINK_LIBRARIES(chat-room muduo_net jsoncpp 
        mysqlclient uuid ssl crypto)
endif()

# 测试程序
# 设置测试源文件
set(TEST_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/test/api_msg_benchmark.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/test/api_msg_benchmark_init.cc
    ${BASE_LIST}
    ${API_LIST}
    ${MYSQL_LIST}
    ${REDIS_LIST}
)

# 添加测试可执行文件
add_executable(api_msg_benchmark ${TEST_SOURCE_FILES})

# 测试程序链接库
target_link_libraries(api_msg_benchmark
    muduo_net
    muduo_base
    jsoncpp
    mysqlclient
    pthread
    uuid
)

add_executable(redis_stream_benchmark test/redis_stream_benchmark.cc  )
target_link_libraries(redis_stream_benchmark
    muduo_net
    muduo_base
    jsoncpp
    hiredis
    pthread
    uuid
    event
)

# # 复制测试配置文件
# configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test/dbserver.conf
#               ${CMAKE_CURRENT_BINARY_DIR}/dbserver.conf
#               COPYONLY)
# configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test/redis.conf
#               ${CMAKE_CURRENT_BINARY_DIR}/redis.conf
#               COPYONLY)    