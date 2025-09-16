INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}../) 

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/client)
INCLUDE_DIRECTORIES(/usr/include/jsoncpp)
 

ADD_EXECUTABLE(client_perf 
    main.cc
    config_manager.cc
)


TARGET_LINK_LIBRARIES(client_perf 
    muduo_net
    jsoncpp
    ssl
    crypto
    muduo_base
    pthread
)
