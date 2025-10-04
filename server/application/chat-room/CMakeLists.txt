INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})


INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/base)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/api)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/service)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/rpc)

INCLUDE_DIRECTORIES(/usr/include/jsoncpp)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/base BASE_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/api API_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/service SERVICE_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/rpc RPC_LIST)

# 根据RPC开关决定是否包含RPC源文件
if(ENABLE_RPC)
    ADD_EXECUTABLE(chat-room main.cc  ${SERVICE_LIST} 
        ${BASE_LIST} ${API_LIST} ${RPC_LIST})
else()
    ADD_EXECUTABLE(chat-room main.cc  ${SERVICE_LIST} 
        ${BASE_LIST} ${API_LIST})
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
        uuid ssl crypto curl
        ${_REFLECTION}
        ${_GRPC_GRPCPP}
        ${_PROTOBUF_LIBPROTOBUF})
else()
    TARGET_LINK_LIBRARIES(chat-room muduo_net jsoncpp 
        uuid ssl crypto curl)
endif()



