# Install script for directory: /home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/lib/libmuduo_base.a")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/muduo/base" TYPE FILE FILES
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/AsyncLogging.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/Atomic.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/BlockingQueue.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/BoundedBlockingQueue.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/Condition.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/CountDownLatch.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/CurrentThread.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/Date.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/Exception.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/FileUtil.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/GzipFile.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/LogFile.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/LogStream.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/Logging.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/Mutex.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/ProcessInfo.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/Singleton.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/StringPiece.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/Thread.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/ThreadLocal.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/ThreadLocalSingleton.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/ThreadPool.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/TimeZone.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/Timestamp.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/Types.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/WeakCallback.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/copyable.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/md5.h"
    "/home/utopianyouth/zerovoice/unit11/myself-chatroom/server/muduo/base/noncopyable.h"
    )
endif()

