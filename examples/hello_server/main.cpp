#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include "web_core/app.hpp"

int main()
{
    try
    {
        web_core::App app;
        app.serveStatic(WEB_ROOT);
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
