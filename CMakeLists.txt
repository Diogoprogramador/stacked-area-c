cmake_minimum_required(VERSION 3.16)
project(stacked_area_c VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(stackedarea STATIC
  src/series.c
  src/stacked.c
  src/render_svg.c)
target_include_directories(stackedarea PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(stackedarea PRIVATE -Wall -Wextra)
endif()

add_executable(sa-cli cli/main.c)
target_link_libraries(sa-cli PRIVATE stackedarea)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(sa-cli PRIVATE -Wall -Wextra)
endif()

enable_testing()

add_executable(test_stacked tests/test_stacked.c)
target_link_libraries(test_stacked PRIVATE stackedarea)
add_test(NAME stacked COMMAND test_stacked)

add_executable(test_svg tests/test_svg.c)
target_link_libraries(test_svg PRIVATE stackedarea)
add_test(NAME svg COMMAND test_svg)

add_test(NAME cli
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.sh
                 $<TARGET_FILE:sa-cli>)
