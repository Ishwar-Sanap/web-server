#pragma once
#include "tcp_listener.hpp"

namespace http_engine{

    class Server
    {
    private:
        TcpListener m_listner;
        void handleClient(int client_fd);
    
    public:
        explicit Server(int port);
        void run();
    };
}