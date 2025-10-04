cmake_minimum_required(VERSION 3.16)
project(ChatServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point to pre-compiled muduo headers and libraries
include_directories(${CMAKE_SOURCE_DIR}/../)
link_directories(${CMAKE_SOURCE_DIR}/../muduo/lib)

# add grpc support, default is off
option(ENABLE_RPC "Enable RPC functionality" ON)

# if gRPC is enabled, add the grpc library
if(ENABLE_RPC)
    add_definitions(-DENABLE_RPC)
endif()

# cpp server components
add_subdirectory(chat-room)

if(ENABLE_RPC)
    add_subdirectory(logic)
    add_subdirectory(job)
endif()
