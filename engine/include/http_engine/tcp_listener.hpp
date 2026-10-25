#pragma once

namespace http_engine
{
    class TcpListener
    {
    private:
        int m_port;
        int m_server_fd;

    public:
        // Listner owns the socket, so to prevent accidental copies.
        TcpListener(const TcpListener &) = delete;
        TcpListener &operator=(const TcpListener &) = delete;

        explicit TcpListener(int port);
        ~TcpListener();

        bool start();
        void stop();
        int acceptClient();
        int fileDescriptor() const;
    };
}