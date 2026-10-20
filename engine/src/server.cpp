#include "server.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include "http_parser.hpp"
#include "response_serializer.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
        http_engine::HttpParser parser;
        http_engine::HttpRequest request;
        http_engine::HttpResponse response;
        http_engine::ResponseSerializer serializer;
        std::size_t bytes_consumed = 0;

        if (parser.parse(received, request, bytes_consumed) != http_engine::ParseResult::COMPLETE)
        {
            std::cout << "Failed to parse Input request try again...\n";
            return;
        }

        std::cout << "Input request recieved from client: " << "\n";
        request.printRequest();

        if (request.m_headers["content-type"] == "text/plain")
        {
            const std::string response_body = "Hello from C++ HTTP Server...!'\n";
            response.m_headers["Content-Type"] = "text/plain";
            response.m_body = response_body;
        }
        else if (request.m_headers["content-type"] == "application/json")
        {
            json jsonInput = json::parse(request.m_body);

            response.m_headers["Content-Type"] = "application/json";
            json jsonOutput;
            jsonOutput["user_name"] = "abc";
            jsonOutput["email"] = "abc@gmail.com";
            jsonOutput["posts"]["data"] = {"001", "002", "003"};

            response.m_body = jsonOutput.dump(); // serialize json object into string
        }

        const std::string raw_resp = serializer.serialize(response);

        std::size_t total_sent = 0;
        while (total_sent < raw_resp.size())
        {
            ssize_t bytes_sent = send(client.fd, raw_resp.data() + total_sent, raw_resp.size() - total_sent, MSG_NOSIGNAL);

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