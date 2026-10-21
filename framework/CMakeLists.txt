add_library(web_core STATIC
   src/app.cpp
   src/router.cpp
)

target_include_directories(web_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(web_core PRIVATE http_engine  PRIVATE nlohmann_json::nlohmann_json)