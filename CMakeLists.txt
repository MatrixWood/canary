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

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules/)

find_package(Filesystem COMPONENTS Final)
if(CXX_FILESYSTEM_HAVE_FS)
    message(STATUS "Found std::filesystem")
endif()

# Check for C++ filesystem support
set(NEED_BOOST_FS 0)
if (DROGON_CXX_STANDARD EQUAL 14)
    # With C++14, use Boost to support any and string_view
    message(STATUS "use c++14")
    find_package(Boost 1.61.0 REQUIRED)
    message(STATUS "Using Boost filesystem, string_view and any")
    message(STATUS "Boost include dir: " ${Boost_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PUBLIC Boost::boost)
    list(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW ${Boost_INCLUDE_DIR})
    set(NEED_BOOST_FS 1)
elseif (DROGON_CXX_STANDARD EQUAL 17)
    # With C++17, use Boost if std::filesystem::path is missing
    message(STATUS "use c++17")

    if (CXX_FILESYSTEM_HAVE_FS)
        set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
        try_compile(check_filesystem_path ${CMAKE_BINARY_DIR}/cmaketest
            ${PROJECT_SOURCE_DIR}/cmake/tests/check_has_std_filesystem_path.cc
            CXX_STANDARD 17)
        set(CMAKE_TRY_COMPILE_TARGET_TYPE)
        if (NOT check_filesystem_path)
          message(STATUS "The std::filesystem seems to be a partial implementation"
            " Falling back to boost::filesystem")
            set(NEED_BOOST_FS 1)
        endif()
    else()
        set(NEED_BOOST_FS 1)
    endif()
else ()
    message(STATUS "use c++20")
endif ()

if(NEED_BOOST_FS)
    find_package(Boost 1.49.0 COMPONENTS filesystem system REQUIRED)
    message(STATUS "Using Boost filesytem::path")
    message(STATUS "Boost include dir: " ${Boost_INCLUDE_DIR})
    include_directories(${BOOST_INCLUDE_DIRS})
    message(STATUS "Boost libraries: " ${Boost_LIBRARIES})
    target_link_libraries(${PROJECT_NAME} PUBLIC Boost::filesystem Boost::system)
    list(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW ${Boost_INCLUDE_DIR})
    option(HAS_STD_FILESYSTEM_PATH "use boost::filesystem" OFF)
else()
    option(HAS_STD_FILESYSTEM_PATH "use std::filesystem" ON)
    # HACK: Needed to be compiled on Yocto Linux
    if(TARGET std::filesystem)
      get_property(CAN_LINK_FS TARGET std::filesystem PROPERTY INTERFACE_LINK_LIBRARIES SET)
      if ( CAN_LINK_FS )
        target_link_libraries(${PROJECT_NAME} PUBLIC std::filesystem)
      endif()
    endif()
endif()

include_directories(base)
include_directories(net)
include_directories(canary)

add_library(
  ${PROJECT_NAME}
  STATIC
  ${PROJECT_SOURCE_DIR}/base/Date.cc
  ${PROJECT_SOURCE_DIR}/base/MsgBuffer.cc
  ${PROJECT_SOURCE_DIR}/base/TimingWheel.cc
  ${PROJECT_SOURCE_DIR}/base/Utility.cc
  ${PROJECT_SOURCE_DIR}/base/Logger.cc
  ${PROJECT_SOURCE_DIR}/base/LogStream.cc
  ${PROJECT_SOURCE_DIR}/base/Md5.cc
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

find_package(UUID REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE UUID_lib)

if (CANARY_OPT_BUILD_UNITTESTS)
  add_subdirectory(unittests)
endif()

if (CANARY_OPT_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()