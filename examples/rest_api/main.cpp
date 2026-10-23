#include <iostream>
#include <cassert>
#include <string>
#include <nlohmann/json.hpp>
#include "web_core/app.hpp"

int main()
{
    web_core::App app;

    app.get("/hello", [](const http_engine::HttpRequest &req, http_engine::HttpResponse &res) {
       res.send("Hello, World");
    });

    app.get("/hello/:Id", [](const http_engine::HttpRequest &req, http_engine::HttpResponse &res) {
        const std::string id = req.param("Id");
        res.send("Hello, World" + id);
    });

    app.get("/users/:id/orders/:orderId", [](const auto&req, auto& res){
        const std::string body = "User id: " + req.param("id") +  " Order id: " + req.param("orderId");
        res.send(body);
    });

    app.get("/hello/errors", [](const auto&req, auto&res){
        res.status(503).send("Service not available..");
        // res.send("Hello"); // If we call send earilier then next time if you call it will gives error
    });

    app.get("/users", [](const auto&req, auto&res){
        res.json({{"id" , "05" }, {"name", "Ishwar"}, {"active", true}});
    });

    app.get("/posts", [](const auto&req, auto&res){
        nlohmann::json body;
        body["userId"] = "01";
        body["name"] = "Ishwar";
        body["posts"]["data"] = {"001", "002", "003"};
        res.json(body);
    });

    app.get("/posts/:id", [](const auto&req, auto&res){
        const auto& id = req.param("id");
        nlohmann::json body;
        body["error"] = "Post with id " + id + " is missing.";
        res.status(404).json(body);
    });

    app.listen(8080);
    
    return 0;
}