cmake_minimum_required(VERSION 3.20)
project(WebServer VERSION 1.0)

set(CMAKE_BUILD_TYPE Release)

set(OUTPUT_DIR "${PROJECT_SOURCE_DIR}/references")

#targets stored in references
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR})  # .a files
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR})  # .so files
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})  # executables

add_subdirectory(engine)
add_subdirectory(framework)
add_subdirectory(examples/hello_server)