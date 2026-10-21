#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "http_engine/http_request.hpp"
#include "http_engine/http_response.hpp"

namespace web_core
{
    using RouteParams = std::unordered_map<std::string, std::string>;

    class Router
    {
    public:
        using Handler = std::function<void(const http_engine::HttpRequest &request,
                                           http_engine::HttpResponse &response)>;

        using QueryParamHandler = std::function<void(const http_engine::HttpRequest &request,
                                                     http_engine::HttpResponse &response,
                                                     const RouteParams &)>;

        void addRoute(const std::string &method, const std::string &pattern, Handler handler);
        void addRoute(const std::string &method, const std::string &pattern, QueryParamHandler handler);

        bool dispatch(const http_engine::HttpRequest &request, http_engine::HttpResponse &response) const;

    private:
        struct Route
        {
            std::string method;
            std::string pattern;
            Handler handler;
            QueryParamHandler paramHandler;
        };

        std::vector<Route> m_routes;

        static bool matchPath(const std::string &pattern, const std::string &path, RouteParams &params);
    };
}