# add grpc support, default is off
option(ENABLE_RPC "Enable RPC functionality" OFF)

# if gRPC is enabled, add the grpc library
if(ENABLE_RPC)
    add_definitions(-DENABLE_RPC)
endif()


# cpp server
add_subdirectory(chat-room)

if(ENABLE_RPC)
    add_subdirectory(logic)
    add_subdirectory(job)
endif()

# cpp client
add_subdirectory(client)

# cpp client performance test
add_subdirectory(client_perf)
