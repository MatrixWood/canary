cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE "Debug")

project(
  canary
  VERSION 0.1.0
  LANGUAGES CXX
)

option(CANARY_OPT_BUILD_UNITTESTS "Build all unittests" ON)
option(CANARY_OPT_BUILD_EXAMPLES "Build all examples" ON)

message(STATUS "CANARY_OPT_BUILD_UNITTESTS is ${CANARY_OPT_BUILD_UNITTESTS}")
message(STATUS "CANARY_OPT_BUILD_EXAMPLES is ${CANARY_OPT_BUILD_EXAMPLES}")

# CMake helpers:
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

include_directories(base)
include_directories(net)

add_library(
  ${PROJECT_NAME}
  STATIC
  ${PROJECT_SOURCE_DIR}/base/Date.cc
  ${PROJECT_SOURCE_DIR}/base/MsgBuffer.cc
  ${PROJECT_SOURCE_DIR}/base/TimingWheel.cc
  ${PROJECT_SOURCE_DIR}/base/Utility.cc
  ${PROJECT_SOURCE_DIR}/base/Logger.cc
  ${PROJECT_SOURCE_DIR}/base/LogStream.cc
  ${PROJECT_SOURCE_DIR}/net/InetAddress.cc
  ${PROJECT_SOURCE_DIR}/net/Channel.cc
  ${PROJECT_SOURCE_DIR}/net/EventLoop.cc
  ${PROJECT_SOURCE_DIR}/net/EventLoopThread.cc
  ${PROJECT_SOURCE_DIR}/net/EventLoopThreadPool.cc
  ${PROJECT_SOURCE_DIR}/net/TcpServer.cc
  ${PROJECT_SOURCE_DIR}/net/TcpClient.cc
  ${PROJECT_SOURCE_DIR}/net/inner/Poller.cc
  ${PROJECT_SOURCE_DIR}/net/inner/EpollPoller.cc
  ${PROJECT_SOURCE_DIR}/net/inner/Timer.cc
  ${PROJECT_SOURCE_DIR}/net/inner/TimerQueue.cc
  ${PROJECT_SOURCE_DIR}/net/inner/Socket.cc
  ${PROJECT_SOURCE_DIR}/net/inner/Acceptor.cc
  ${PROJECT_SOURCE_DIR}/net/inner/Connector.cc
  ${PROJECT_SOURCE_DIR}/net/inner/TcpConnectionImpl.cc
)

if (CANARY_OPT_BUILD_UNITTESTS)
  add_subdirectory(unittests)
endif()

if (CANARY_OPT_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()