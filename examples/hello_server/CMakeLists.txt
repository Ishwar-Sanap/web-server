add_executable(hello_server
    main.cpp
)

target_include_directories(hello_server
    PUBLIC
       ${CMAKE_PROJECT_SOURCE_DIR}/engine/include
)

target_link_libraries(hello_server PRIVATE http_engine PRIVATE nlohmann_json::nlohmann_json)