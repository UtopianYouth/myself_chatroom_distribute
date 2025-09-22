INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})
INCLUDE_DIRECTORIES(/usr/local/include)
INCLUDE_DIRECTORIES(/usr/include/mysql)
INCLUDE_DIRECTORIES(/usr/include/jsoncpp)

AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR} MAIN_LIST)

# Find Protobuf installation
if(ENABLE_RPC)
    # Looks for protobuf-config.cmake file installed by Protobuf's cmake installation.
    set(protobuf_MODULE_COMPATIBLE TRUE)
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
    find_package(gRPC CONFIG REQUIRED)
    message(STATUS "Using gRPC ${gRPC_VERSION}")
    
    set(_GRPC_GRPCPP gRPC::grpc++)
    if(CMAKE_CROSSCOMPILING)
        find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
    else()
        set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
    endif()
endif()

ADD_EXECUTABLE(logic ${MAIN_LIST})

# 根据RPC开关决定链接的库
if(ENABLE_RPC)
    TARGET_LINK_LIBRARIES(logic muduo_net jsoncpp mysqlclient uuid ssl crypto rdkafka rdkafka++
        ${_REFLECTION}
        ${_GRPC_GRPCPP}
        ${_PROTOBUF_LIBPROTOBUF})
else()
    TARGET_LINK_LIBRARIES(logic muduo_net jsoncpp mysqlclient uuid ssl crypto rdkafka rdkafka++)
endif()