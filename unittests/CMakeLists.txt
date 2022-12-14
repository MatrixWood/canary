find_program(CMAKE_MEMORYCHECK_COMMAND valgrind)
set(memcheck_command ${CMAKE_MEMORYCHECK_COMMAND} ${CMAKE_MEMORYCHECK_COMMAND_OPTIONS} --error-exitcode=1 --leak-check=full)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

enable_testing()
find_package(GTest REQUIRED)

set(CANARY_TEST_LIST
  DateUnittest
  InetAddressUnittest
  LoggerUnittest
  TimingWheelUnittest
)

foreach(src ${CANARY_TEST_LIST})
  message(STATUS "unittest files found: ${src}.cc")
  add_executable(${src} EXCLUDE_FROM_ALL ${src}.cc)
  target_include_directories(${src} PUBLIC ${PROJECT_SOURCE_DIR})
  target_link_libraries(${src} GTest::gtest GTest::gtest_main canary)
  add_test(${src} ${src})
  add_dependencies(check ${src})
endforeach()

foreach(src ${CANARY_TEST_LIST})
  add_test(${src}-memory-check ${memcheck_command} ./${src})
endforeach()