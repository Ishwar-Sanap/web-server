#pragma once
#include <memory>
#include "router.hpp"
#include "http_engine/server.hpp"
#include "http_engine/http_parser.hpp"
#include "http_engine/response_serializer.hpp"
#include "http_engine/string_consts.hpp"

namespace web_core
{
    class App
    {
    private:
        std::shared_ptr<web_core::Router> m_router;
        std::unique_ptr<http_engine::Server> m_server;
    public:
        App();
        void listen(int port);
        void get(const std::string &pattern, Router::Handler handler);
        void post(const std::string &pattern, Router::Handler handler);
        void put(const std::string &pattern, Router::Handler handler);
        void patch(const std::string &pattern, Router::Handler handler);
        void delete_(const std::string &pattern, Router::Handler handler);
    };
}