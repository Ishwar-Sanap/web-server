#include "http_engine/http_request.hpp"
#include <iostream>

namespace http_engine
{
    HttpRequest::HttpRequest() {}
    HttpRequest::HttpRequest(const std::string &method, const std::string &path, const std::string &version, const std::unordered_map<std::string, std::string> &headers, const std::string &body) : m_method(method), m_path(path), m_version(version), m_headers(std::move(headers)), m_body(body)
    {
    }

    std::string HttpRequest::param(const std::string &key) const
    {
        return m_params.at(key);
    }

    void HttpRequest::setParams(const RouteParams &params)
    {
        m_params = std::move(params);
    }

    std::string HttpRequest::method() const
    {
        return m_method;
    }
    std::string HttpRequest::path() const
    {
        return m_path;
    }
    std::string HttpRequest::body() const
    {
        return m_body;
    }
    std::string HttpRequest::version() const
    {
        return m_version;
    }
    const std::unordered_map<std::string, std::string> &HttpRequest::headers() const
    {
        return m_headers;
    }
    std::string HttpRequest::header(const std::string &key) const
    {
        if (m_headers.find(key) != m_headers.end())
            return m_headers.at(key);

        else return "";
    }
    void HttpRequest::printRequest()
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
}