#pragma once

#include "http_response.hpp"

namespace http_engine
{
    class ResponseSerializer
    {
    public:
        std::string serialize(const HttpResponse &response) const;
    };

}