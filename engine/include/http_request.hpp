#pragma once

#include <string>
#include <unordered_map>

namespace http_engine
{

    class HttpRequest
    {
    public:
        std::string m_method;
        std::string m_path;
        std::string m_version;

        std::unordered_map<std::string, std::string> m_headers;
        std::string m_body;
    };
}