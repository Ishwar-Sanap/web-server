add_library(http_engine STATIC
    src/server.cpp
    src/tcp_listener.cpp
)

target_include_directories(http_engine
    PUBLIC
       ${CMAKE_CURRENT_SOURCE_DIR}/include
)
