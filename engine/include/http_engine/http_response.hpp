#pragma once

#include <string>
#include <unordered_map>
#include "string_consts.hpp"
#include <nlohmann/json.hpp>

namespace http_engine
{
    class HttpResponse
    {
    private:
        bool m_header_sent = false;
        std::string m_version = string_consts::http_version_1_1; // HTTP protocol version, such as HTTP/1.1
        int m_status_code = 200;                                 // Numeric result, such as 200, 404, or 500
        std::string m_reason = string_consts::reason_ok;         // Text associated with the status code
        std::unordered_map<std::string, std::string> m_headers;  // Metadata describing the response
        std::string m_body;                                      // Actual response content
    public:
        HttpResponse &setHeader(const std::string &name, const std::string &value);
        HttpResponse &send(const std::string &body);
        HttpResponse &json(const nlohmann::json &body);
        HttpResponse &status(int code);
        std::string version() const;
        int statusCode() const;
        std::string reason() const;
        std::string body() const;
        const std::unordered_map<std::string, std::string> &headers() const;
        bool hasHeaderSent() const;
    };
}