#pragma once

#include <string>
#include <unordered_map>

namespace http_engine
{
    class HttpResponse
    {
    public:
        int m_status_code = 200;
        std::string m_reason_phrase = "OK";

        std::unordered_map<std::string, std::string> m_headers;
        std::string m_body;

        //serialize() function will convert the response object into the bytes sent through the TCP socket
        std::string serialize() const;
    };
}