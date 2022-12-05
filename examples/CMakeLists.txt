
find_program(CMAKE_MEMORYCHECK_COMMAND valgrind)
set(memcheck_command ${CMAKE_MEMORYCHECK_COMMAND} ${CMAKE_MEMORYCHECK_COMMAND_OPTIONS} --error-exitcode=1 --leak-check=full)

set(CANARY_EXAMPLES
)

foreach(src ${CANARY_EXAMPLES})
  message(STATUS "example files found: ${src}.cc")
  add_executable(${src} EXCLUDE_FROM_ALL ${src}.cc)
  target_include_directories(${src} PUBLIC ${PROJECT_SOURCE_DIR})
  target_link_libraries(${src} canary)
endforeach()

foreach(src ${CANARY_EXAMPLES})
  add_test(${src}-memory-check ${memcheck_command} ./${src})
endforeach()