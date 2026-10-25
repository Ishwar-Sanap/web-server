#pragma once
#include "tcp_listener.hpp"
#include "web_core/router.hpp"
#include <memory>

namespace http_engine
{
    class Server
    {
    private:
        TcpListener m_listener;
        std::shared_ptr<web_core::Router> m_router;

    public:
        explicit Server(int port);
        void run();
        void setRouter(std::shared_ptr<web_core::Router> router);
    };
}