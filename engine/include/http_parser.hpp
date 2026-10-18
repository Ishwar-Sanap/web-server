#pragma once

#include "http_request.hpp"

namespace http_engine
{
    enum class ParseResult
    {
        COMPLETE,
        INCOMPLETE,
        BADREQUEST
    };

    class HttpParser
    {
    public:
        ParseResult parse(std::string_view raw_request, HttpRequest &request, std::size_t &bytes_consumed) const;
    };

}