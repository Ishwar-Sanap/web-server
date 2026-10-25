#include <iostream>
#include <stdexcept>
#include <string>
#include "web_core/app.hpp"

int main()
{
    try
    {
        web_core::App app;
        const std::string dir = "/home/ishwar/web-server/public";
        app.serveStatic(dir);
        // app.serveStatic(WEB_ROOT); // WEB_ROOT --> Dir path defined through CMake
        std::cout << "Web page available at http://localhost:8080\n";
        app.listen(8080);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
