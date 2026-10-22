#include <iostream>
#include <cassert>
#include <string>
#include <nlohmann/json.hpp>
#include "web_core/app.hpp"

using json = nlohmann::json;

int main()
{
    // http_engine::Server myserver(8080);
    // myserver.run();

    web_core::App app;

    app.get("/hello", [](const http_engine::HttpRequest &req, http_engine::HttpResponse &res) {
        res.m_body = "Hello, World!";
    });

    app.get("/hello/:Id", [](const http_engine::HttpRequest &req, http_engine::HttpResponse &res) {
        const std::string id = req.param("Id");
        res.m_body = "Hello, World! " + id;
    });

    app.listen(8080);
    return 0;
}