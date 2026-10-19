#pragma once

#include <string>
#include <unordered_map>

namespace http_engine
{
    struct HttpRequest
    {
    public:
        std::string m_method;  // HTTP methods : [GET, POST, PUT, DELETE, PATCH]
        std::string m_path;    // URL of resources ex: /users
        std::string m_version; // HTTP Version ex: HTTP/1.1
        std::unordered_map<std::string, std::string> m_headers; // Metadata about input request
        std::string m_body;                                     // Actual data passed through input request.
    };
}