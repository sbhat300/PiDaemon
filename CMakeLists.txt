
cmake_minimum_required(VERSION 3.10.0)
project(Communications)

set(PROJECT_SOURCES     ${PROJECT_SOURCE_DIR}/src/main.c)

add_executable(Communications ${PROJECT_SOURCES})

target_include_directories(Communications PUBLIC ${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(Communications ${CMAKE_SOURCE_DIR}/lib/spicenet/libspicenet.a)