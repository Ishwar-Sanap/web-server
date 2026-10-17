#include "server.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

namespace http_engine
{

    Server::Server(int port) : m_listner(port)
    {
    }

    void Server::run()
    {
        if (!m_listner.start())
        {
            std::cerr << "Failed to start server\n";
            return;
        }

        while (true)
        {
            int client_fd = m_listner.acceptClient();

            if (client_fd == -1)
            {
                if (errno == EINTR)
                    continue;

                // accept error shouldn't necessarily terminate the entire server
                continue;
            }

            handleClient(client_fd);
        }
    }

    void Server::handleClient(int client_fd)
    {
        struct ClientSocket
        {
            int fd;

            ~ClientSocket()
            {
                if (fd != -1)
                {
                    close(fd);
                }
            }
        } client{client_fd};

        char buffer[4096];

        // read data sent by client
        ssize_t bytes_received;

        do
        {
            bytes_received = recv(client.fd, buffer, sizeof(buffer), 0);
        } while (bytes_received == -1 && errno == EINTR);

        if (bytes_received == 0)
        {
            std::cout << "Client disconnected without sending data\n";
            return;
        }

        if (bytes_received == -1)
        {
            std::cerr << "recv() failed: " << std::strerror(errno) << "\n";
            return;
        }

        std::string received(buffer, bytes_received);
        std::cout << "Data recieved from client: " << received << "\n";

        const std::string response = "Hello from C++ HTTP Server...!'\n";

        std::size_t total_sent = 0;
        while (total_sent < response.size())
        {
            ssize_t bytes_sent = send(client.fd, response.data() + total_sent, response.size() - total_sent, MSG_NOSIGNAL);

            if (bytes_sent == -1 && errno == EINTR)
            {
                continue;
            }

            if (bytes_sent <= 0)
            {
                std::cerr << "send() failed or connection closed " << "\n";
                return;
            }

            total_sent += static_cast<std::size_t>(bytes_sent);
        }

        std::cout << "Response sent successfully\n";
    }

}