#include "http_engine/tcp_listener.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace http_engine
{
    TcpListener::TcpListener(int port) : m_port(port), m_server_fd(-1)
    {
    }

    TcpListener::~TcpListener()
    {
        stop();
    }

    bool TcpListener::start()
    {
        if (m_server_fd != -1)
            return true;

        m_server_fd = socket(AF_INET, SOCK_STREAM, 0); // create an IPv4 tcp socket

        if (m_server_fd == -1)
        {
            std::cerr << "socket() failed: " << std::strerror(m_server_fd) << "\n";
            return false;
        }

        // Allow the port to be re used after restarting the server
        int opt = 1;
        if (setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        {
            std::cerr << "setsockopt() failed: " << std::strerror(errno) << "\n";
            stop();
            return false;
        }

        // configure address and port
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(static_cast<unsigned short>(m_port));

        // Associate the socket with port
        if (bind(m_server_fd, (struct sockaddr *)&address, sizeof(address)) == -1)
        {
            std::cerr << "bind() failed: " << std::strerror(errno) << "\n";
            stop();
            return false;
        }

        // Mark the socket as ready to accept connections.
        if (listen(m_server_fd, SOMAXCONN) == -1)
        {
            std::cerr << "listen() failed: " << std::strerror(errno) << "\n";
            stop();
            return false;
        }

        std::cout << "Server is listening on port : " << m_port << "\n";
        return true;
    }

    int TcpListener::acceptClient()
    {
        if (m_server_fd == -1)
        {
            std::cerr << "Cannot accept: listener is not started \n";
            return -1;
        }

        sockaddr_in client_address{};
        socklen_t client_length = sizeof(client_address);

        // Wait for client connection
        int client_fd = accept(m_server_fd, (struct sockaddr *)&client_address, &client_length);

        if (client_fd == -1)
        {
            if (errno != EINTR)
            {
                std::cerr << "accept() failed: " << std::strerror(errno) << "\n";
            }
            return -1;
        }

        std::cout << "Client connected fd: " << client_fd << " \n";
        return client_fd;
    }

    void TcpListener::stop()
    {
        if (m_server_fd != -1)
        {
            close(m_server_fd);
            m_server_fd = -1;
        }
    }
}