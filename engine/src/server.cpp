#include "server.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include "http_parser.hpp"
#include "response_serializer.hpp"
#include "string_consts.hpp"
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

    EngineReturnValue Server::readCompleteRequest(int client_fd, HttpRequest &request, HttpParser &parser)
    {
        std::size_t bytes_consumed = 0;
        char buffer[4096];
        std::string received_data;

        while (true)
        {
            ssize_t bytes_received;
            do
            {
                bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
            } while (bytes_received == -1 && errno == EINTR);

            if (bytes_received == 0)
            {
                std::cout << "Client disconnected before sending a complete request\n";
                return http_engine::EngineReturnValue::CLIENT_CONNECTION_CLOSED;
            }

            if (bytes_received == -1)
            {
                std::cerr << "recv() failed: " << std::strerror(errno) << "\n";
                return http_engine::EngineReturnValue::IO_ERROR;
            }

            received_data.append(buffer, static_cast<std::size_t>(bytes_received));

            const http_engine::ParseResult parse_result = parser.parse(received_data, request, bytes_consumed);
            if (parse_result == http_engine::ParseResult::COMPLETE)
            {
                break;
            }

            if (parse_result == http_engine::ParseResult::BADREQUEST)
            {
                return http_engine::EngineReturnValue::BAD_REQUEST;
            }
        }

        return http_engine::EngineReturnValue::SUCCESS;
    }

    void Server::sendCompleteResponse(int client_fd, const std::string &raw_resp)
    {
        std::size_t total_sent = 0;
        while (total_sent < raw_resp.size())
        {
            ssize_t bytes_sent = send(client_fd, raw_resp.data() + total_sent, raw_resp.size() - total_sent, MSG_NOSIGNAL);

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

        http_engine::HttpParser parser;
        http_engine::HttpRequest request;
        http_engine::HttpResponse response;
        http_engine::ResponseSerializer serializer;

        http_engine::EngineReturnValue read_result = readCompleteRequest(client.fd, request, parser);

        if (read_result == http_engine::EngineReturnValue::CLIENT_CONNECTION_CLOSED ||
            read_result == http_engine::EngineReturnValue::IO_ERROR)
        {
            return;
        }
        else if (read_result == http_engine::EngineReturnValue::BAD_REQUEST)
        {
            response.m_status_code = 400;
            response.m_reason = string_consts::reason_bad_request;
            response.m_headers[string_consts::content_type_header] = string_consts::media_type_application_json;
            json jsonOutput;
            jsonOutput["error"] = string_consts::reason_bad_request;
            jsonOutput["message"] = "The request body contains invalid JSON or missing required fields.";

            response.m_body = jsonOutput.dump(); // serialize json object into string
        }
        else if (read_result == http_engine::EngineReturnValue::SUCCESS)
        {
            // std::cout << "Input request recieved from client: " << "\n";
            // request.printRequest();

            if (request.m_headers[string_consts::content_type_header_lower] == string_consts::media_type_text_plain)
            {
                const std::string response_body = "Hello from C++ HTTP Server...!'\n";
                response.m_headers[string_consts::content_type_header] = string_consts::media_type_text_plain;
                response.m_body = response_body;
            }
            else if (request.m_headers[string_consts::content_type_header_lower] == string_consts::media_type_application_json)
            {
                try
                {
                    json jsonInput = json::parse(request.m_body);

                    response.m_headers[string_consts::content_type_header] = string_consts::media_type_application_json;
                    json jsonOutput;
                    jsonOutput["user_name"] = "abc";
                    jsonOutput["email"] = "abc@gmail.com";
                    jsonOutput["posts"]["data"] = {"001", "002", "003"};

                    response.m_body = jsonOutput.dump(); // serialize json object into string
                }
                catch (const std::exception &e)
                {
                    std::cerr << e.what() << '\n';

                    //Added here temporarily to send Bad request response.
                    response.m_status_code = 400;
                    response.m_reason = string_consts::reason_bad_request;
                    response.m_headers[string_consts::content_type_header] = string_consts::media_type_application_json;
                    json jsonOutput;
                    jsonOutput["error"] = string_consts::reason_bad_request;
                    jsonOutput["message"] = "Failed to parse JSON data.";
                    response.m_body = jsonOutput.dump(); // serialize json object into string
                }
            }
        }

        const std::string raw_resp = serializer.serialize(response);

        sendCompleteResponse(client.fd, raw_resp);

        std::cout << "Response sent successfully\n";
    }

}