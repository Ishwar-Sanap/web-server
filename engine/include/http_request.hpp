#pragma once

#include <string>
#include <unordered_map>
#include <iostream>

namespace http_engine
{
    struct HttpRequest
    {
    public:
        std::string m_method;                                   // HTTP methods : [GET, POST, PUT, DELETE, PATCH]
        std::string m_path;                                     // URL of resources ex: /users
        std::string m_version;                                  // HTTP Version ex: HTTP/1.1
        std::unordered_map<std::string, std::string> m_headers; // Metadata about input request
        std::string m_body;                                     // Actual data passed through input request.

        void printRequest()
        {
            std::string req;
            req += m_method + " " + m_path + " " + m_version + "\r\n";

            for (const auto &[name, value] : m_headers)
            {
                req += name + ": " + value + "\r\n";
            }

            req += "\r\n";
            req += m_body;

            std::cout << req << std::endl;
        }
    };
}