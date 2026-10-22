#include "http_engine/response_serializer.hpp"
#include "http_engine/string_consts.hpp"
#include <stdexcept>

namespace http_engine
{

    std::string ResponseSerializer::serialize(const HttpResponse &response) const
    {
        if (response.version() != string_consts::http_version_1_0 &&
            response.version() != string_consts::http_version_1_1)
            throw std::invalid_argument("Unsupported HTTP version");

        if (response.statusCode() < 100 || response.statusCode() > 599)
            throw std::invalid_argument("Invalid HTTP status code");

        std::string output;
        output.reserve(256 + response.body().size());

        // building the response status line.
        output += response.version() + " ";
        output += std::to_string(response.statusCode()) + " ";
        output += response.reason() + "\r\n";

        bool has_content_length = false;
        // Serialize response headers.
        for (const auto &[name, value] : response.headers())
        {
            if (name.empty() || name.find_first_of("\r\n:") != std::string::npos || value.find_first_of("\r\n") != std::string::npos)
                throw std::invalid_argument("Invalid HTTP header");

            output += name + ": " + value + "\r\n";

            if (name == string_consts::content_length_header ||
                name == string_consts::content_length_header_lower)
                has_content_length = true;
        }

        if (!has_content_length)
        {
            output += "Content-Length: " + std::to_string(response.body().size()) + "\r\n";
        }

        // Add empty line to seperates headers from body.
        output += "\r\n";

        output += response.body();

        return output;
    }
}