#include <iostream>
#include "http_engine/server.hpp"
#include "http_engine/http_parser.hpp"
#include "http_engine/response_serializer.hpp"
#include "http_engine/string_consts.hpp"
#include <cassert>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main()
{
    http_engine::Server myserver(8080);
    myserver.run();

    return 0;
}