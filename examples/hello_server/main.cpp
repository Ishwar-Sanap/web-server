#include "server.hpp"
#include <iostream>
int main()
{
    http_engine::Server myserver(8080);
    myserver.run();

    return 0;
}