#include "web_core/router.hpp"
#include <sstream>
#include <utility>
#include <vector>

namespace web_core
{
    std::vector<std::string> splitPath(const std::string &path)
    {
        std::vector<std::string> segments;
        std::stringstream stream(path);
        std::string segment;

        while (std::getline(stream, segment, '/'))
        {
            if (!segment.empty())
            {
                segments.push_back(segment);
            }
        }

        return segments;
    }

    void Router::addRoute(const std::string &method, const std::string &pattern, Handler handler)
    {
        m_routes.push_back({method, pattern, std::move(handler), {}});
    }
    void Router::addRoute(const std::string &method, const std::string &pattern, QueryParamHandler handler)
    {
        m_routes.push_back({method, pattern, {}, std::move(handler)});
    }

    // Compare the incoming path with the registered pattern one segment at a time
    bool Router::matchPath(const std::string &pattern, const std::string &path, RouteParams &params)
    {
        const auto expected = splitPath(pattern);
        const auto actual = splitPath(path);

        if (expected.size() != actual.size())
            return false;

        RouteParams captured;

        for (std::size_t i = 0; i < expected.size(); i++)
        {
            const auto &routeSegment = expected[i];
            const auto &pathSegement = actual[i];

            // pattern  -->  /user/:id
            // path     -->  /user/123

            // routeSegment  --> [user, :id]
            // pathsegment --> [user, 123]

            if (!routeSegment.empty() && routeSegment[0] == ':')
            {
                const std::string name = routeSegment.substr(1);

                if (name.empty())
                    return false;

                // storing the id -> 123 in map
                captured[name] = pathSegement;
            }
            else if (routeSegment != pathSegement)
            {
                return false;
            }
        }

        params = std::move(captured);
        return true;
    }

    bool Router::dispatch(const http_engine::HttpRequest &request, http_engine::HttpResponse &response) const
    {
        // First pass: exact routes take precedence.
        for (const auto &route : m_routes)
        {
            if (route.method == request.m_method && route.pattern == request.m_path)
            {
                route.handler(request, response);
                return true;
            }
        }

        // Second pass: parameterized routes.
        for (const auto &route : m_routes)
        {
            if (route.method != request.m_method)
            {
                continue;
            }

            RouteParams params;

            if (matchPath(route.pattern, request.m_path, params))
            {
                route.paramHandler(request, response, params);
                return true;
            }
        }

        return false;
    }
};

/*
router.addRoute(
    "GET",
    "/users/:id",
    [](const auto& req, auto& res, const auto& params) {
        const std::string& id = params.at("id");

        nlohmann::json result = {
            {"id", id},
            {"message", "User requested"}
        };

        // Adapt to your Response interface.
        res.setHeader("Content-Type", "application/json");
        res.setBody(result.dump());
    }
);

For a request to GET /users/101, the handler receives params["id"] == "101".

For GET /users/205, the same handler receives "205".

The router matches the path; your application decides whether the ID represents a valid user.
*/