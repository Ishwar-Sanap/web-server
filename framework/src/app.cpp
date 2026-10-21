#include "web_core/app.hpp"

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

    void App::get(const std::string &pattern, Router::QueryParamHandler handler)
    {
        m_router->addRoute("GET", pattern, handler);
    }
}