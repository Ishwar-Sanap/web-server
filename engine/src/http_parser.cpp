#include "http_parser.hpp"
#include <iostream>
#include <string>
#include <string_view>
#include <limits>
#include <system_error>
#include <cctype>
#include <charconv>
#include "string_consts.hpp"

namespace http_engine
{
    constexpr std::size_t MAX_HEADER_SIZE = 16 * 1024; // 16 KiB
    constexpr std::size_t MAX_BODY_SIZE = 1024 * 1024; // 1 MiB

    bool is_token_char(unsigned char ch)
    {
        if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))
            return true;

        constexpr std::string_view punctucation = "!#$%&'*+-.^_`|~";
        return punctucation.find(static_cast<char>(ch)) != std::string_view::npos;
    }
    bool valid_method(std::string_view method)
    {
        if (method.empty())
            return false;

        for (unsigned char ch : method)
        {
            if (!is_token_char(ch))
                return false;
        }

        return true;
    }

    bool valid_header_value(std::string_view value)
    {
        for (unsigned char ch : value)
        {
            if ((ch < 0x20 && ch != '\t') || ch == 0x7f)
            {
                return false;
            }
        }

        return true;
    }

    bool parsed_content_length(std::string_view value, std::size_t &result)
    {
        if (value.empty())
            return false;

        std::size_t parsed = 0;

        const char *begin = value.data();
        const char *end = begin + value.size();

        //std::from_chars parses characters from a range into a number. 
        //It doesn't need a null-terminated string, and it reports both whether parsing succeeded and where it stopped.
        auto [ptr, error] = std::from_chars(begin, end, parsed);

        if (error != std::errc{} || ptr != end)
            return false;

        result = parsed;
        return true;
    }
    std::string_view trim_ows(std::string_view text)
    {
        // ows = optional whitespace: space or horizontal tab.

        while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
        {
            text.remove_prefix(1);
        }

        while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
        {
            text.remove_suffix(1);
        }

        return text;
    }

    std::string to_lower(std::string_view text)
    {
        std::string result;
        result.reserve(text.size());
        for (unsigned char ch : text)
        {
            if (ch >= 'A' && ch <= 'Z')
            {
                ch = static_cast<unsigned char>(ch + ('a' - 'A'));
            }
            result.push_back(ch);
        }

        return result;
    }

    ParseResult HttpParser::parse(std::string_view raw_request, HttpRequest &request, std::size_t &bytes_consumed) const
    {
        bytes_consumed = 0;
        const std::size_t header_end = raw_request.find("\r\n\r\n");

        if (header_end == std::string_view::npos)
        {
            if (raw_request.size() > MAX_HEADER_SIZE)
            {
                return ParseResult::BADREQUEST;
            }

            for (std::size_t i = 0; i < raw_request.size(); i++)
            {
                if (raw_request[i] == '\n' && (i == 0 || raw_request[i - 1] != '\r'))
                    return ParseResult::BADREQUEST;

                if (raw_request[i] == '\r' && i + 1 < raw_request.size() && raw_request[i + 1] != '\n')
                    return ParseResult::BADREQUEST;
            }

            return ParseResult::INCOMPLETE;
        }

        if (header_end + 4 > MAX_HEADER_SIZE)
        {
            return ParseResult::BADREQUEST;
        }

        HttpRequest parsed_request;

        // ----- Parse request line -----
        const std::size_t request_line_end = raw_request.find("\r\n");

        if (request_line_end == std::string_view::npos || request_line_end > header_end)
        {
            return ParseResult::BADREQUEST;
        }

        const std::string_view request_line = raw_request.substr(0, request_line_end);

        // Require exaclty 3 parts seprated by single spaces.
        // ex:--> "GET /hello HTTP/1.1"
        const std::size_t first_space = request_line.find(' ');

        if (first_space == std::string_view::npos || first_space == 0)
            return ParseResult::BADREQUEST;

        const std::size_t second_space = request_line.find(' ', first_space + 1);

        if (second_space == std::string_view::npos || second_space == first_space + 1 || second_space + 1 >= request_line.size())
            return ParseResult::BADREQUEST;

        // Third space
        if (request_line.find(' ', second_space + 1) != std::string_view::npos)
            return ParseResult::BADREQUEST;

        const std::string_view method = request_line.substr(0, first_space);
        const std::string_view path = request_line.substr(first_space + 1, second_space - first_space - 1);
        const std::string_view version = request_line.substr(second_space + 1);

        if (!valid_method(method) || path.find_first_of(" \t\r\n") != std::string_view::npos || path.find_first_of("\r\n") != std::string_view::npos)
            return ParseResult::BADREQUEST;

        if (version != string_consts::http_version_1_0 && version != string_consts::http_version_1_1)
            return ParseResult::BADREQUEST;

        parsed_request.m_method = std::string(method);
        parsed_request.m_path = std::string(path);
        parsed_request.m_version = std::string(version);

        // ----- Parse headers -----

        std::size_t position = request_line_end + 2;
        std::size_t content_length = 0;
        bool has_content_length = false;

        while (position < header_end)
        {
            const std::size_t line_end = raw_request.find("\r\n", position);

            if (line_end == std::string_view::npos || line_end > header_end)
                return ParseResult::BADREQUEST;

            const std::string_view line = raw_request.substr(position, line_end - position);

            if (line.empty() || line.front() == ' ' || line.front() == '\t')
                return ParseResult::BADREQUEST;

            const std::size_t colon = line.find(':');

            if (colon == std::string_view::npos || colon == 0)
                return ParseResult::BADREQUEST;

            const std::string_view name = line.substr(0, colon);
            const std::string_view value = trim_ows(line.substr(colon + 1));

            // check name and value are correct
            for (unsigned char ch : name)
            {
                if (!is_token_char(ch))
                    return ParseResult::BADREQUEST;
            }

            if (!valid_header_value(value))
                return ParseResult::BADREQUEST;

            const std::string lower_name = to_lower(name);

            // Will implement later
            if (lower_name == "transfer-encoding")
            {
                return ParseResult::BADREQUEST;
            }

            if (lower_name == string_consts::content_length_header_lower)
            {
                std::size_t parsed_length = 0;

                if (!parsed_content_length(value, parsed_length))
                    return ParseResult::BADREQUEST;

                // Reject duplicate content-length headers
                if (has_content_length)
                    return ParseResult::BADREQUEST;

                content_length = parsed_length;
                has_content_length = true;
            }

            // stored name and value in the map
            if (!parsed_request.m_headers.emplace(lower_name, std::string(value)).second)
                return ParseResult::BADREQUEST;

            position = line_end + 2;
        }

        // HTTP/1.1 request must includes exacly one Host header.
        if (parsed_request.m_version == string_consts::http_version_1_1 &&
            parsed_request.m_headers.find(string_consts::host_header) == parsed_request.m_headers.end())
            return ParseResult::BADREQUEST;

        if (content_length > MAX_BODY_SIZE)
            return ParseResult::BADREQUEST;

        const std::size_t body_start = header_end + 4;

        // protect against size arithmatic overflow
        if (content_length > std::numeric_limits<std::size_t>::max() - body_start)
            return ParseResult::BADREQUEST;

        const std::size_t total_size = body_start + content_length;

        // Whole content is not arrived yet
        if (raw_request.size() < total_size)
            return ParseResult::INCOMPLETE;

        parsed_request.m_body = std::string(raw_request.substr(body_start, content_length));

        // valid request available
        request = std::move(parsed_request);
        bytes_consumed = total_size;

        return ParseResult::COMPLETE;
    }
}

// GET /hello HTTP/1.1
// Host: localhost:8080
// User-Agent: curl/8.0
// Accept: */*
