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

include_directories(canary)
include_directories(canary/base)
include_directories(canary/net)
include_directories(canary/http)
include_directories(canary/mysql)
include_directories(canary/redis)

add_library(
  ${PROJECT_NAME}
  STATIC
  ${PROJECT_SOURCE_DIR}/canary/base/Date.cc
  ${PROJECT_SOURCE_DIR}/canary/base/MsgBuffer.cc
  ${PROJECT_SOURCE_DIR}/canary/base/TimingWheel.cc
  ${PROJECT_SOURCE_DIR}/canary/base/Utility.cc
  ${PROJECT_SOURCE_DIR}/canary/base/Logger.cc
  ${PROJECT_SOURCE_DIR}/canary/base/LogStream.cc
  ${PROJECT_SOURCE_DIR}/canary/base/Md5.cc

  ${PROJECT_SOURCE_DIR}/canary/net/InetAddress.cc
  ${PROJECT_SOURCE_DIR}/canary/net/Channel.cc
  ${PROJECT_SOURCE_DIR}/canary/net/EventLoop.cc
  ${PROJECT_SOURCE_DIR}/canary/net/EventLoopThread.cc
  ${PROJECT_SOURCE_DIR}/canary/net/EventLoopThreadPool.cc
  ${PROJECT_SOURCE_DIR}/canary/net/TcpServer.cc
  ${PROJECT_SOURCE_DIR}/canary/net/TcpClient.cc
  ${PROJECT_SOURCE_DIR}/canary/net/inner/Poller.cc
  ${PROJECT_SOURCE_DIR}/canary/net/inner/EpollPoller.cc
  ${PROJECT_SOURCE_DIR}/canary/net/inner/Timer.cc
  ${PROJECT_SOURCE_DIR}/canary/net/inner/TimerQueue.cc
  ${PROJECT_SOURCE_DIR}/canary/net/inner/Socket.cc
  ${PROJECT_SOURCE_DIR}/canary/net/inner/Acceptor.cc
  ${PROJECT_SOURCE_DIR}/canary/net/inner/Connector.cc
  ${PROJECT_SOURCE_DIR}/canary/net/inner/TcpConnectionImpl.cc

  ${PROJECT_SOURCE_DIR}/canary/http/HttpUtils.cc
)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/cmake_modules/)

find_package(UUID REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE UUID_lib)

find_package(ZLIB REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)

if(TARGET std::filesystem)
  target_link_libraries(${PROJECT_NAME} PUBLIC std::filesystem)
endif()

if (CANARY_OPT_BUILD_UNITTESTS)
  add_subdirectory(unittests)
endif()

if (CANARY_OPT_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()