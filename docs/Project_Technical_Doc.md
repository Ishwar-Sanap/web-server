# Web Server Project: Technical Documentation

## 1. Project summary

This repository implements a small HTTP/1.x web server and a lightweight C++ web
framework on top of it. It is designed as a learning and demonstration project:
the networking engine handles TCP connections and HTTP messages, while the
`web_core` framework gives application code routes, middleware, JSON responses,
and static-file serving.

The implementation is Linux-specific. It uses IPv4 TCP sockets, `epoll`, and
`eventfd`; it is not a portable cross-platform server. The examples provide a
static website and an in-memory JSON REST API.

### At a glance

| Area | Current implementation |
| --- | --- |
| Language | C++17 |
| Build system | CMake (minimum version 3.20) |
| Network protocol | HTTP/1.0 and HTTP/1.1 request parsing; HTTP/1.1 response serialization |
| Transport | IPv4 TCP sockets |
| I/O model | Nonblocking sockets and a Linux `epoll` event loop |
| Application work | Fixed-size worker pool, configured for 2–8 threads |
| JSON | `nlohmann/json` 3.12.0, fetched by CMake |
| Included applications | Static website and in-memory REST API |
| Automated tests | No test targets or test source files are currently defined |
| External service APIs | None; API in this project means the framework interface and its demo HTTP routes |

## 2. Architecture and request lifecycle

The repository has two main libraries:

1. **`http_engine`** owns the TCP listener, event loop, HTTP parser, request and
   response types, and response serializer.
2. **`web_core`** provides `App` and `Router`, connecting developer-defined
   middleware and route handlers to the engine.

For each client connection, the server:

1. Accepts a nonblocking socket and registers it with `epoll`.
2. Reads available bytes into a per-connection input buffer.
3. Parses the request. Incomplete requests remain buffered until more data
   arrives; malformed requests receive a `400 Bad Request`.
4. Submits a complete request to a worker thread. The worker runs middleware,
   dispatches a route, and serializes the response.
5. Places the completed response in a synchronized completion queue and signals
   an `eventfd`.
6. The event-loop thread handles the completion notification, writes the
   response to the client socket (including partial writes), and closes the
   connection.

Workers do not read or write client sockets. This keeps socket operations and
connection state in the event-loop thread while allowing application handlers
to run concurrently.

### Connection and workload limits

The current server implementation sets these limits in `engine/src/server.cpp`
and `engine/src/http_parser.cpp`:

| Limit or behavior | Value |
| --- | --- |
| Read buffer per receive | 4 KiB |
| Maximum HTTP header section | 16 KiB |
| Maximum parsed body | 1 MiB |
| Maximum accumulated connection request bytes | 16 KiB + 1 MiB + 4 KiB |
| Client idle timeout | 60 seconds |
| Worker threads | Between 2 and 8, based on hardware concurrency |
| Pending application jobs | At most 1,024 |
| Bytes represented by pending jobs | At most 64 MiB |
| Epoll events returned per wait | At most 128 per call; this is not a total client limit |
| Requests per connection | One; the response includes `Connection: close` |

When the application-work limits are reached, the server responds with
`503 Server Busy`. The worker-pool implementation is vendored in
`engine/include/http_engine/ctpl_stl.h`; the server applies the pending-work
limits before queuing request handlers.

## 3. Repository layout

| Path | Responsibility |
| --- | --- |
| `CMakeLists.txt` | Project configuration, C++ standard, output paths, and subdirectories |
| `engine/include/http_engine/` | Public HTTP engine types and declarations |
| `engine/src/` | TCP listener, event loop, HTTP parser, request/response behavior, and serialization |
| `framework/include/web_core/` | Public `App` and `Router` interfaces |
| `framework/src/` | Route dispatch, middleware, and static-file serving |
| `examples/hello_server/` | Example app serving the files under `public/` |
| `examples/rest_api/` | Example REST app and in-memory demo data |
| `public/` | HTML, CSS, JavaScript, and image served by the website example |
| `docs/` | Project notes and implementation guides |
| `benchmark_server.py` | Optional asyncio client for loopback-only load/latency measurements |
| `benchmark_report.md` | A recorded local benchmark run, if present in the checkout |
| `build/` | Local CMake build directory; generated files are not source |
| `references/` | Configured output directory for static libraries and example executables |

## 4. Dependencies and tools

### Required to build

- A Linux environment with the POSIX socket APIs and Linux `epoll`, `eventfd`,
  and `accept4` interfaces.
- CMake 3.20 or newer.
- A C++ compiler with C++17 support.
- A working build tool generated/selected by CMake (the checked-in build
  configuration currently generates Makefiles).
- Network access on the first configure if `nlohmann/json` is not already
  available in CMake's FetchContent cache. The root build downloads the 3.12.0
  release archive from the upstream GitHub release URL.

### Optional tools

- Python 3 and its standard library to run `benchmark_server.py`; the script
  uses `asyncio` and permits only loopback destinations.
- `curl` or another HTTP client for manual endpoint checks.

There is no database, authentication provider, external HTTP API, TLS library,
or frontend package manager configured in this repository. The UI uses browser
HTML, CSS, and JavaScript directly.

## 5. Build and run

From the repository root:

```sh
cmake -S . -B build
cmake --build build --parallel
```

The root CMake file sets C++17 and currently forces the `Debug` build type. It
places libraries and executables under `references/`:

- `references/libhttp_engine.a`
- `references/libweb_core.a`
- `references/hello_server`
- `references/rest_api`

Run one example at a time because both listen on port 8080:

```sh
./references/hello_server
```

Then visit `http://localhost:8080/`. The website's button changes page text in
the browser; it does not make an API request.

```sh
./references/rest_api
```

The REST example exposes the endpoints in Section 8. The server binds to
`INADDR_ANY` on IPv4, so it listens on the configured port on all local IPv4
interfaces, not only loopback.

### Current example path caveat

`examples/hello_server/main.cpp` currently passes the absolute directory
`/home/ishwar/web-server/public` to `serveStatic`. Although its CMake target
defines `WEB_ROOT`, the example does not use that definition. On another
checkout path, update the example to use the configured web root (or pass the
correct public directory) before running it. The REST example does not depend
on that directory.

There are currently no CMake install/export rules, so these targets are built
for use in this source tree rather than installed as a packaged SDK.

## 6. Framework API for application developers

The main public interface is `web_core::App` in
`framework/include/web_core/app.hpp`.

```cpp
#include "web_core/app.hpp"

int main()
{
    web_core::App app;
    app.get("/hello", [](const http_engine::HttpRequest &req,
                         http_engine::HttpResponse &res) {
        res.send("Hello");
    });
    app.listen(8080);
}
```

### `App` methods

| Method | Purpose |
| --- | --- |
| `get(pattern, handler)` | Register a GET route |
| `post(pattern, handler)` | Register a POST route |
| `put(pattern, handler)` | Register a PUT route |
| `patch(pattern, handler)` | Register a PATCH route |
| `delete_(pattern, handler)` | Register a DELETE route; the suffix avoids the C++ keyword |
| `use(middleware)` | Register global middleware, run in registration order |
| `serveStatic(directory)` | Register middleware that serves GET files from a directory |
| `listen(port)` | Create and run the HTTP server on the given port |

Route handlers have the type:

```cpp
void(const http_engine::HttpRequest &, http_engine::HttpResponse &)
```

### Request API

`http_engine::HttpRequest` exposes:

- `method()`, `path()`, `version()`, and `body()`.
- `headers()` to access the complete header map.
- `header(name)` to retrieve one header. Parsed header names are normalized to
  lowercase, so lookups should use lowercase names.
- `param(name)` for a captured route parameter.
- `query(name)` for a parsed query parameter.

`param()` and `query()` use map lookup that throws if a key is absent. Check
whether a parameter is present in application logic before accessing optional
values; the current request API does not provide a non-throwing lookup method.

Route parameters use colon-prefixed path segments, for example
`/users/:id/orders/:orderId`. Query strings are split on `&` and `=`; percent
encoding is not decoded, and repeated keys overwrite earlier values.

### Response API

`http_engine::HttpResponse` provides chainable:

- `status(code)` to set a status code and its reason phrase for the common
  statuses defined by the implementation.
- `setHeader(name, value)` to set or replace a response header.
- `send(text)` to set a text/plain body unless a content type was already set.
- `json(value)` to serialize a `nlohmann::json` value and set
  `Content-Type: application/json`.

The response serializer adds `Content-Length` when it is not already present
and rejects unsupported response protocol versions, out-of-range status codes,
or invalid header values. A response can only be sent once; attempting to send
or set another header after `send()`/`json()` throws.

### Routing and middleware behavior

- Exact method/path registrations are checked before parameterized routes.
- Route matching is case-sensitive and supports one named parameter per
  colon-prefixed segment.
- If the path matches a route but no registered method matches, dispatch
  returns `405 Method Not Allowed` and an `Allow` header.
- If no route path matches, the server returns `404 Not Found`.
- Middleware is global and runs before route matching, in registration order.
  Returning `true` continues to the next middleware or route. Returning
  `false` stops dispatch and must follow a response send; returning false
  without a sent response results in a `500`.
- `serveStatic()` registers such middleware. A matching file is sent and stops
  further dispatch; missing or non-file paths continue to later middleware
  and route lookup. `/` maps to `index.html`. The implementation canonicalizes
  requested paths and checks that the resolved path remains under the
  canonicalized root.

## 7. HTTP engine behavior

### Request parsing

`HttpParser` incrementally parses HTTP/1.0 and HTTP/1.1 request data. It
distinguishes complete, incomplete, and malformed input. It checks request-line
shape, method token characters, header names and values, and `Content-Length`.
For HTTP/1.1, exactly one `Host` header is required by the parser's header map.
Duplicate headers and duplicate `Content-Length` are rejected. `Transfer-Encoding`
is currently rejected; chunked request bodies are not supported.

The parser accepts a body up to 1 MiB. A request using another HTTP version,
malformed syntax, or an oversized header/body is rejected. The parsed URL path
is separated from its query string before routing.

### Responses and connection handling

The server serializes HTTP responses, handles partial socket writes, and uses
`MSG_NOSIGNAL` so writing to a disconnected peer does not deliver `SIGPIPE`.
Each connection is closed after a single response. Persistent HTTP keep-alive
and multiple pipelined requests on one connection are not implemented.

### Linux event-loop and concurrency primitives

- `socket()`, `setsockopt()`, `bind()`, `listen()`, and `accept4()` manage IPv4
  TCP sockets. The listener and clients are nonblocking and close-on-exec.
- `epoll_create1()`, `epoll_ctl()`, and `epoll_wait()` monitor readiness.
  The event loop is level-triggered; it does not enable `EPOLLET`.
- `eventfd` wakes the event loop when a worker has completed a request.
- A mutex protects the worker completion queue.
- A generation number helps the event loop discard a worker result for a client
  descriptor that has since been closed/reused.

## 8. Included HTTP API example

The REST example listens on port `8080` and stores data only in memory. The
initial `users` and `posts` collections are defined in
`examples/rest_api/dummy_data.hpp`; changes disappear when the process exits.
The application uses a mutex around its shared data store because route
handlers can run on separate worker threads.

| Method | Path | Behavior |
| --- | --- | --- |
| `GET` | `/hello` | Returns `Hello, World` |
| `GET` | `/hello/:Id` | Returns a greeting with the captured `Id` |
| `GET` | `/users/:id/orders/:orderId` | Returns the user and order IDs in text |
| `GET` | `/hello/errors` | Example `503 Service Unavailable` response |
| `GET` | `/users` | Returns the user collection as JSON |
| `GET` | `/users/:id` | Returns one user, or JSON 404 if absent |
| `POST` | `/users` | Creates a user from a JSON object and assigns an ID; returns 201 |
| `PUT` | `/users/:id` | Replaces an existing user object; the path ID is retained |
| `PATCH` | `/users/:id` | Updates fields on an existing user; body `id` is ignored |
| `DELETE` | `/users/:id` | Deletes a user or returns JSON 404 if absent |
| `GET` | `/posts` | Returns all posts and echoes `page`, `limit`, and `sort` query values |
| `GET` | `/posts/:id` | Returns one post, or JSON 404 if absent |

For POST, PUT, and PATCH, the example expects a valid JSON object. Invalid JSON
or a non-object body returns a JSON `400` response. The collection store is a
demonstration helper, not a database or durable persistence layer.

Example requests:

```sh
curl -i http://127.0.0.1:8080/users
curl -i 'http://127.0.0.1:8080/posts?page=1&limit=10&sort=asc'
curl -i -X POST http://127.0.0.1:8080/users \
  -H 'Content-Type: application/json' \
  -d '{"name":"Katherine Johnson","email":"katherine@example.com","active":true}'
curl -i -X PATCH http://127.0.0.1:8080/users/1 \
  -H 'Content-Type: application/json' \
  -d '{"active":false}'
```

## 9. Static website example

`examples/hello_server` serves the `public/` directory through
`App::serveStatic()`. The page consists of:

- `public/index.html`: landing page markup.
- `public/style.css`: responsive styling.
- `public/app.js`: client-side button interaction; it changes page text locally.
- `public/assets/template.png`: displayed image asset.

The static handler recognizes HTML, CSS, JavaScript, JSON, text, common image
formats, and PDF extensions. Other files use `application/octet-stream`.

## 10. Benchmarking

`benchmark_server.py` is an optional asynchronous Python client. It opens one
TCP connection per request, sends one HTTP request, reads until the server
closes the connection, and reports status counts, throughput, latency
percentiles, connection timing, and client-side resource measurements. It
resolves the destination and refuses non-loopback addresses; it is intended
for localhost measurements, not a remote load-testing tool.

Example:

```sh
python3 benchmark_server.py \
  --host 127.0.0.1 --port 8080 --path /users \
  --requests 10000 --concurrency 100
```

The workspace report `benchmark_report.md` records one run with
10,000 requests and concurrency 1,000, reporting 3,318.80 valid responses per
second. Treat that as a result for that specific machine, build, route, and
configuration—not as a general performance guarantee. Its CPU and RSS numbers
are for the Python client; server resource consumption must be measured
separately. Benchmark results are sensitive to client capacity, operating
system, build type, and concurrency.

## 11. Current scope and limitations

The following are implementation facts and important integration constraints,
not claims that the project provides production-grade support:

- Linux-only event notification and IPv4-only listener setup.
- No TLS/HTTPS, authentication, authorization, database, persistence, or
  external service integration.
- One request per TCP connection; no keep-alive or HTTP pipelining.
- No chunked transfer encoding, multipart support, or URL percent-decoding.
- No built-in structured logging/configuration system; diagnostics are written
  to standard output/error.
- No install/export packaging rules and no automated test suite in the current
  CMake project.
- The static website example currently contains a checkout-specific absolute
  path (see Section 5).

Before exposing the server to untrusted networks or using it for production
traffic, review resource limits, input validation, operational logging,
shutdown behavior, TLS, authentication, and test coverage for the intended
deployment.
