cmake_minimum_required(VERSION 3.10)
project(data-structure-for-redis)

set(CMAKE_C_STANDARD 99)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-g -O2 -DNDEBUG")

add_subdirectory(intset)
