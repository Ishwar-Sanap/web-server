
include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

add_library(http_engine STATIC
    src/server.cpp
    src/tcp_listener.cpp
    src/http_parser.cpp
    src/response_serializer.cpp
)

target_include_directories(http_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(http_engine PRIVATE nlohmann_json::nlohmann_json)