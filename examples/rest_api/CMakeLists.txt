add_executable(rest_api
    main.cpp
)

target_include_directories(rest_api
    PUBLIC
       ${CMAKE_PROJECT_SOURCE_DIR}/engine/include
)

target_link_libraries(rest_api PRIVATE http_engine PRIVATE web_core PRIVATE nlohmann_json::nlohmann_json)