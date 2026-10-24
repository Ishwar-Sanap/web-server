cmake_minimum_required(VERSION 3.20)
project(WebServer VERSION 1.0)

# set(CMAKE_BUILD_TYPE Release)
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 17) # Use C++17 standard
set(CMAKE_CXX_STANDARD_REQUIRED ON) # Enforce C++17 standard
set(OUTPUT_DIR "${PROJECT_SOURCE_DIR}/references")

#targets stored in references
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR})  # .a files
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR})  # .so files
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})  # executables

add_subdirectory(engine)
add_subdirectory(framework)
add_subdirectory(examples/hello_server)
add_subdirectory(examples/rest_api)