#pragma once
#include "tcp_listener.hpp"
#include "http_parser.hpp"
#include "engine_enums.hpp"
#include "web_core/router.hpp"
#include <memory>
namespace http_engine
{

    class Server
    {
    private:
        TcpListener m_listner;
        std::shared_ptr<web_core::Router> m_router;
        void handleClient(int client_fd);
        EngineReturnValue readCompleteRequest(int client_fd, HttpRequest &request, HttpParser &parser);
        void sendCompleteResponse(int client_fd, const std::string &raw_resp);
    public:
        explicit Server(int port);
        void run();
        void setRouter(std::shared_ptr<web_core::Router> router);
    };
}