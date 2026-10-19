#pragma once

#include <string>
#include <unordered_map>

namespace http_engine
{
    struct HttpResponse
    {
    public:
        std::string m_version = "HTTP/1.1"; // HTTP protocol version, such as HTTP/1.1
        int m_status_code = 200;            // Numeric result, such as 200, 404, or 500
        std::string m_reason = "OK";        // Text associated with the status code
        std::unordered_map<std::string, std::string> m_headers; // Metadata describing the response
        std::string m_body; // Actual response content
    };
}