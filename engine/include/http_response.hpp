#pragma once

#include <string>
#include <unordered_map>
#include "string_consts.hpp"

namespace http_engine
{
    struct HttpResponse
    {
    public:
        std::string m_version = string_consts::http_version_1_1; // HTTP protocol version, such as HTTP/1.1
        int m_status_code = 200;            // Numeric result, such as 200, 404, or 500
        std::string m_reason = string_consts::reason_ok; // Text associated with the status code
        std::unordered_map<std::string, std::string> m_headers; // Metadata describing the response
        std::string m_body; // Actual response content
    };
}