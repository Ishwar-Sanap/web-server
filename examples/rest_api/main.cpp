#include "server.hpp"
#include <iostream>
#include "http_parser.hpp"
#include "response_serializer.hpp"
#include "string_consts.hpp"
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