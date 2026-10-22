#pragma once

#include <string>
#include <unordered_map>

namespace http_engine
{
    using RouteParams = std::unordered_map<std::string, std::string>;
    class HttpRequest
    {
    private:
        RouteParams m_params;
        std::string m_method;                                   // HTTP methods : [GET, POST, PUT, DELETE, PATCH]
        std::string m_path;                                     // URL of resources ex: /users
        std::string m_version;                                  // HTTP Version ex: HTTP/1.1
        std::unordered_map<std::string, std::string> m_headers; // Metadata about input request
        std::string m_body;                                     // Actual data passed through input request.
    public:
        HttpRequest();
        HttpRequest(const std::string &method, const std::string &path, const std::string &version,const std::unordered_map<std::string, std::string> &headers, const std::string &body);
        void printRequest();
        std::string method() const;
        std::string path() const;
        std::string body() const;
        std::string version() const;
        const std::unordered_map<std::string, std::string> &headers() const;
        std::string header(const std::string &key) const;
        void setParams(const RouteParams &params);
        std::string param(const std::string &key) const;
    };
}