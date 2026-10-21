#pragma once
#include "tcp_listener.hpp"
#include "http_parser.hpp"
#include "engine_enums.hpp"

namespace http_engine
{

    class Server
    {
    private:
        TcpListener m_listner;
        void handleClient(int client_fd);
        EngineReturnValue readCompleteRequest(int client_fd, HttpRequest &request, HttpParser &parser);
        void sendCompleteResponse(int client_fd, const std::string &raw_resp);
    public:
        explicit Server(int port);
        void run();
    };
}