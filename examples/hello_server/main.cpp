#include <iostream>
#include "http_engine/server.hpp"
#include "http_engine/http_parser.hpp"
#include "http_engine/response_serializer.hpp"
#include "http_engine/string_consts.hpp"
#include <cassert>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
void TestHttpRequestResponse()
{
    /*
    Raw Input request string look like this:

    "GET /hello HTTP/1.1\r\nHost: localhost:8080\r\nUser-Agent: test-client\r\n\r\n"
    "POST /users HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length: 12\r\n\r\nHello World!"

    /************************************
    HTTP - Request
    -------------------------------------
    GET /hello HTTP/1.1
    Host: localhost:8080

    HTTP - Response
    -------------------------------------
    HTTP/1.1 200 OK
    Content-Type: text/plain
    Content-Length: 13
    Connection: close

    Hello, World!
    ****************************************

    Raw HTTP Response look like this:
    "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\nHello, World!"

    */

    http_engine::HttpParser parser;
    http_engine::HttpRequest request;
    std::size_t consumed = 0;

    // const std::string raw =
    //     "GET /hello HTTP/1.1\r\n"
    //     "Host: localhost:8080\r\n"
    //     "User-Agent: test-client\r\n"
    //     "\r\n";

    // std::cout<<raw<<std::endl;  // "GET /hello HTTP/1.1\r\nHost: localhost:8080\r\nUser-Agent: test-client\r\n\r\n"
    // const auto result = parser.parse(raw, request, consumed);

    // assert(result == http_engine::ParseResult::COMPLETE);
    // assert(request.m_method == "GET");
    // assert(request.m_path == "/hello");
    // assert(request.m_version == "HTTP/1.1");
    // assert(request.m_headers.at("host") == "localhost:8080");
    // assert(request.m_headers.at("user-agent") == "test-client");
    // assert(request.m_body.empty());
    // assert(consumed == raw.size());

    const std::string raw =
        "POST /users HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "Hi";

    const auto result = parser.parse(raw, request, consumed);

    assert(result == http_engine::ParseResult::INCOMPLETE);
    assert(consumed == 0);

    const std::string raw2 =
        "POST /users HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "Hello, World!";

    const auto result2 = parser.parse(raw2, request, consumed);
    assert(result2 == http_engine::ParseResult::COMPLETE);
    assert(request.m_body == "Hello, World!");

    http_engine::HttpResponse response;
    response.m_status_code = 200;
    response.m_reason = http_engine::string_consts::reason_ok;
    response.m_headers[http_engine::string_consts::content_type_header] =
        http_engine::string_consts::media_type_text_plain;
    response.m_body = "Hello, World!";

    http_engine::ResponseSerializer serializer;
    std::string raw_response = serializer.serialize(response);

    std::cout << raw_response << std::endl;
}

int main()
{
    http_engine::Server myserver(8080);
    myserver.run();

    // TestHttpRequestResponse();

    // json input;
    // input["user_name"] = "abc";
    // input["email"] = "abc@gmail.com";
    // input["posts"]["data"] = {"001", "002", "003"};

    // std::cout<<input<<std::endl;
    return 0;
}