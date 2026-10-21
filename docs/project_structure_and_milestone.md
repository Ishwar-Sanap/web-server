Project structure:

cpp-http-framework/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── LICENSE
│
│
├── examples/
│   ├── hello_server/
│   │   ├── CMakeLists.txt
│   │   └── main.cpp
│   └── rest_api/
│       ├── CMakeLists.txt
│       └── main.cpp
│
├── engine/
│   ├── CMakeLists.txt
│   ├── include/http_engine/
│   │   ├── server.hpp
│   │   ├── tcp_listener.hpp
│   │   ├── event_loop.hpp
│   │   ├── connection.hpp
│   │   ├── http_parser.hpp
│   │   ├── request.hpp
│   │   ├── response.hpp
│   │   └── config.hpp
│   └── src/
│       ├── server.cpp
│       ├── tcp_listener.cpp
│       ├── event_loop_epoll.cpp
│       ├── connection.cpp
│       ├── http_parser.cpp
│       ├── response.cpp
│       └── config.cpp
│
├── framework/
│   ├── CMakeLists.txt
│   ├── include/web_framework/
│   │   ├── app.hpp
│   │   ├── router.hpp
│   │   ├── middleware.hpp
│   │   ├── context.hpp
│   │   ├── static_files.hpp
│   │   └── json.hpp
│   └── src/
│       ├── app.cpp
│       ├── router.cpp
│       ├── middleware.cpp
│       ├── context.cpp
│       └── static_files.cpp
│
├── common/
│   ├── include/common/
│   │   ├── logger.hpp
│   │   ├── thread_pool.hpp
│   │   └── error.hpp
│   └── src/
│       ├── logger.cpp
│       └── thread_pool.cpp
│
└── www/
    ├── index.html
    ├── style.css
    └── app.js


Milestones :

Build it in 10 milestones
Follow this order. Every milestone should produce something working and testable before you move to the next.

Milestone 1 — Create a basic TCP server
First working version
Learn socket(), bind(), listen(), accept(), recv() and send().
Deliverable: a client can connect to localhost:8080 and receive a response.




Milestone 2 — Implement HTTP parsing and responses
Parse the request line, headers, and body. Create HttpRequest and HttpResponse types.
Deliverable: valid requests receive proper HTTP status lines, headers, and bodies.




Milestone 3 — Implement routing
Map HTTP methods and paths to C++ handlers. Return 404 Not Found when a route does not exist and 405 Method Not Allowed when the path exists but the method is unsupported.
Deliverable: GET /hello and GET /users invoke different handlers.




Milestone 4 — Add static files and REST APIs
Serve HTML, CSS, JavaScript, and images. Add POST, PUT, and DELETE routes.
Deliverable: a browser can open a web page, and Postman can call a REST API.




Milestone 5 — Implement the non-blocking event loop
Set sockets to non-blocking mode and use Linux epoll to monitor readiness events.
Deliverable: one event loop can manage many connections without blocking on an individual client.




Milestone 6 — Add robust connection management
Handle partial reads/writes, multiple requests per connection, timeouts, disconnects, and keep-alive.
Deliverable: connections work correctly under slow clients and repeated requests.




Milestone 7 — Add a thread pool
Build a bounded task queue, worker threads, synchronization, and a clear shutdown protocol.
Deliverable: expensive application work can run without stalling the I/O event loop.




Milestone 8 — Build your web framework
Add App, Router, middleware, request context, JSON responses, and a clean developer-facing API.
Deliverable: another developer can build an API without writing socket code.




Milestone 9 — Harden and test
Add request-size limits, parser validation, safe file paths, error handling, configuration, logging, graceful shutdown, and automated tests.
Deliverable: predictable behavior under malformed requests, disconnects, and resource limits.




Milestone 10 — Benchmark and package
Measure throughput, latency, memory usage, and error rates. Package the engine and framework as reusable libraries.
Deliverable: benchmark reports, installable headers/libraries, and example applications.


This sequence follows the progression from basic networking through HTTP, epoll, threading, production features, and benchmarking in your project notes. Project-Context.txt

