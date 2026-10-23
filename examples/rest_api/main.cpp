#include <iostream>
#include <string>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "web_core/app.hpp"
#include "dummy_data.hpp"

int main()
{
    web_core::App app;
    rest_api::DummyDataStore data;

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

    app.get("/users", [&data](const auto &, auto &res) {
        res.json(data.list("users"));
    });

    app.get("/users/:id", [&data](const auto &req, auto &res) {
        const nlohmann::json *user = data.find("users", req.param("id"));
        if (user == nullptr)
        {
            res.status(404).json({{"error", "User not found"}});
            return;
        }
        res.json(*user);
    });

    //GET /posts?page=1&limit=10&sort=asc
    app.get("/posts", [&data](const auto &req, auto &res) {
        try
        {
            const std::string page = req.query("page");
            const std::string limit = req.query("limit");
            const std::string sort = req.query("sort");
            
            res.json({{"data", data.list("posts")}, {"page", page} , {"limit", limit} , {"sort", sort}});
        }
        catch(const std::exception& e)
        {
            res.status(404).json({{"error", "Query param not found"}});
            return;
        }
        
    });

    //GET /posts/1
    app.get("/posts/:id", [&data](const auto &req, auto &res) {
        const nlohmann::json *post = data.find("posts", req.param("id"));
        if (post == nullptr)
        {
            res.status(404).json({{"error", "Post not found"}});
            return;
        }
        res.json(*post);
    });

    app.post("/users", [&data](const auto &req, auto &res) {
        try
        {
            const nlohmann::json user = nlohmann::json::parse(req.body());
            const nlohmann::json created_user = data.create("users", user);
            res.status(201).json({{"message", "User created successfully"}, {"user", created_user}});
        }
        catch (const nlohmann::json::parse_error &)
        {
            res.status(400).json({{"error", "Request body must contain valid JSON"}});
        }
        catch (const std::invalid_argument &)
        {
            res.status(400).json({{"error", "Request body must be a JSON object"}});
        }
    });

    app.put("/users/:id", [&data](const auto &req, auto &res) {
        try
        {
            const nlohmann::json user = nlohmann::json::parse(req.body());
            nlohmann::json *replaced_user = data.replace("users", req.param("id"), user);
            if (replaced_user == nullptr)
            {
                res.status(404).json({{"error", "User not found"}});
                return;
            }
            res.json(*replaced_user);
        }
        catch (const nlohmann::json::parse_error &)
        {
            res.status(400).json({{"error", "Request body must contain valid JSON"}});
        }
        catch (const std::invalid_argument &)
        {
            res.status(400).json({{"error", "Request body must be a JSON object"}});
        }
    });

    app.patch("/users/:id", [&data](const auto &req, auto &res) {
        try
        {
            const nlohmann::json fields = nlohmann::json::parse(req.body());
            nlohmann::json *updated_user = data.update("users", req.param("id"), fields);
            if (updated_user == nullptr)
            {
                res.status(404).json({{"error", "User not found"}});
                return;
            }
            res.json(*updated_user);
        }
        catch (const nlohmann::json::parse_error &)
        {
            res.status(400).json({{"error", "Request body must contain valid JSON"}});
        }
        catch (const std::invalid_argument &)
        {
            res.status(400).json({{"error", "Request body must be a JSON object"}});
        }
    });

    app.delete_("/users/:id", [&data](const auto &req, auto &res) {
        if (!data.erase("users", req.param("id")))
        {
            res.status(404).json({{"error", "User not found"}});
            return;
        }
        res.status(200).json({{"message", "User deleted successfully"}});
    });

    app.listen(8080);
    
    return 0;
}