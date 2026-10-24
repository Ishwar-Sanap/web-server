#include "web_core/app.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
namespace fs = std::filesystem;

namespace
{
    std::string getContentType(const fs::path &path)
    {
        static const std::unordered_map<std::string, std::string> types = {
            {".html", "text/html; charset=utf-8"},
            {".htm", "text/html; charset=utf-8"},
            {".css", "text/css; charset=utf-8"},
            {".js", "application/javascript"},
            {".json", "application/json"},
            {".txt", "text/plain; charset=utf-8"},
            {".png", "image/png"},
            {".jpg", "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".gif", "image/gif"},
            {".svg", "image/svg+xml"},
            {".ico", "image/x-icon"},
            {".pdf", "application/pdf"},
            {".webp", "image/webp"}};

        auto it = types.find(path.extension().string());

        if (it != types.end())
            return it->second;

        return "application/octet-stream";
    }

    bool isInsideDirectory(const fs::path &root, const fs::path &file)
    {
        const fs::path relative = file.lexically_relative(root);
        if (relative.empty())
            return file == root;

        return *relative.begin() != "..";
    }
}

namespace web_core
{
    App::App()
    {
        m_router = std::make_shared<web_core::Router>();
    };

    void App::listen(int port)
    {
        m_server = std::make_unique<http_engine::Server>(port);
        m_server->setRouter(m_router);
        m_server->run();
    }

    void App::get(const std::string &pattern, Router::Handler handler)
    {
        m_router->addRoute("GET", pattern, handler);
    }

    void App::post(const std::string &pattern, Router::Handler handler)
    {
        m_router->addRoute("POST", pattern, handler);
    }

    void App::put(const std::string &pattern, Router::Handler handler)
    {
        m_router->addRoute("PUT", pattern, handler);
    }

    void App::patch(const std::string &pattern, Router::Handler handler)
    {
        m_router->addRoute("PATCH", pattern, handler);
    }

    void App::delete_(const std::string &pattern, Router::Handler handler)
    {
        m_router->addRoute("DELETE", pattern, handler);
    }

    void App::use(Router::Middleware middleware)
    {
        m_router->addMiddleWare(middleware);
    }

    void App::serveStatic(const std::string &directory)
    {
        const fs::path root = fs::canonical(directory);

        use([root](const http_engine::HttpRequest &req, http_engine::HttpResponse &res){
            if (req.method() != "GET")
                return true;

            std::string path = req.path();
            if (path.empty() || path.front() != '/')
                return true;

            path.erase(0, 1);
            if (path.empty())
                path = "index.html";

            std::error_code ec;
            const fs::path filePath = fs::weakly_canonical(root / path, ec);
            if (ec || !isInsideDirectory(root, filePath) || !fs::is_regular_file(filePath, ec) || ec)
                return true;

            std::ifstream file(filePath, std::ios::binary);
            if (!file)
                return true;

            const std::string body{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
            res.setHeader("Content-Type", getContentType(filePath)).send(body);
            return false;
        });
    }
}