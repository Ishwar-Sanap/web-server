# How the Linux epoll Server Works

This guide explains the current server implementation in `engine/src/server.cpp`,
the listener in `engine/src/tcp_listener.cpp`, and how the `ctpl::thread_pool`
is used to run route handlers without blocking network I/O.

The server is Linux-specific because it uses `epoll`, `eventfd`, and `accept4`.
The current design handles many open client connections with one network event
loop, while a fixed-size pool runs request handlers concurrently.

## 1. High-level architecture

The server has two kinds of work:

1. **Network I/O**, owned by one event-loop thread:
   - accept new clients;
   - receive request bytes;
   - parse complete HTTP requests;
   - send response bytes;
   - add, modify, and remove file descriptors from epoll.
2. **Application work**, run by worker threads:
   - middleware and route dispatch;
   - application handler code;
   - response serialization.

Workers do not operate on client sockets. When a worker has completed a
response, it puts the result into a synchronized queue and signals an
`eventfd`. The event loop receives that notification through epoll, retrieves
the response, and sends it to the client.

```text
client
  │
  ▼
nonblocking socket ── readiness ──► epoll event loop
                                      │ parse complete request
                                      ▼
                                  worker queue
                                      │
                                      ▼
                             worker runs middleware
                             and route handler
                                      │ serialized response
                                      ▼
                             completion queue
                                      │ eventfd signal
                                      ▼
epoll event loop ◄────────────────────┘
  │ register EPOLLOUT and send response
  ▼
client
```

## 2. What “nonblocking” means here

A blocking `recv()` waits until bytes arrive, and a blocking `send()` may wait
for the peer to read data. If the server did either operation for one client,
it could stop making progress on all the others.

The listener and accepted sockets are created as nonblocking. Calls such as
`accept4()`, `recv()`, and `send()` return immediately when they cannot make
progress. The event loop uses epoll to wait until a descriptor is ready.

Readiness is not a promise that an entire HTTP request or response can be
processed at once:

- a readable socket may contain only part of a request;
- a writable socket may accept only part of a response.

The connection stores buffers and offsets so processing can continue on a
later readiness notification.

## 3. Starting the listening socket

`TcpListener::start()` in `engine/src/tcp_listener.cpp` creates and prepares the
server socket.

### `socket()`

```cpp
socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)
```

- `AF_INET` selects IPv4 addresses.
- `SOCK_STREAM` selects TCP.
- `SOCK_NONBLOCK` makes socket operations return instead of waiting.
- `SOCK_CLOEXEC` marks the descriptor close-on-exec, so a subsequently
  executed child process does not accidentally inherit it.
- `0` lets the system select the default TCP protocol.

### `setsockopt()`

```cpp
setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))
```

`SO_REUSEADDR` permits the address to be reused when restarting the server
under the usual TCP rules. It does not allow two unrelated servers to take
over an already-active listener.

### `bind()` and `listen()`

`bind()` associates the socket with the configured port and `INADDR_ANY`, which
means the server listens on the machine's IPv4 interfaces. `listen()` changes
the socket into a passive listening socket.

The backlog passed to `listen()` is for connections waiting to be accepted. It
is not a limit on the number of already accepted, active clients.

### `accept4()`

When the listener becomes readable, the event loop calls `acceptClient()`,
which uses:

```cpp
accept4(listener_fd, ..., SOCK_NONBLOCK | SOCK_CLOEXEC)
```

This creates a new socket for one client and applies the nonblocking and
close-on-exec flags to it. Since the listener itself is nonblocking,
`accept4()` returns `EAGAIN` or `EWOULDBLOCK` when its pending-connection queue
is empty. `EINTR` means the call was interrupted and can be retried.

## 4. Creating the epoll event loop

`Server::run()` performs the following setup:

1. Starts the TCP listener.
2. Calls `epoll_create1(EPOLL_CLOEXEC)` to create an epoll instance.
3. Registers the listener with `epoll_ctl(..., EPOLL_CTL_ADD, ...)` and
   `EPOLLIN`.
4. Creates a nonblocking `eventfd` for worker completion notifications.
5. Registers that `eventfd` with epoll for `EPOLLIN`.
6. Enters the `epoll_wait()` loop.

### The `epoll_ctl()` arguments

The general form used by the server is:

```cpp
epoll_ctl(epoll_fd, operation, target_fd, &event)
```

- `epoll_fd`: the epoll instance that tracks descriptors.
- `operation`: what to do with the target descriptor.
- `target_fd`: the listener, a client socket, or the completion `eventfd`.
- `event`: the readiness flags to watch and a value to return with the event.

The code uses these operations:

- `EPOLL_CTL_ADD`: begin monitoring a descriptor.
- `EPOLL_CTL_MOD`: replace the events currently monitored for a client.
- `EPOLL_CTL_DEL`: stop monitoring a client before closing it.

Each registration sets `event.data.fd`. `epoll_wait()` returns that value so
the event loop can identify which descriptor is ready.

### Readiness flags used

| Flag | Meaning in this server |
| --- | --- |
| `EPOLLIN` | The listener can accept clients, a client has bytes to read, or the `eventfd` has a completion notification to consume. |
| `EPOLLOUT` | A client socket can accept response bytes. The event loop watches this only after a response is ready. |
| `EPOLLRDHUP` | The peer closed or shut down its write side. The server includes it when registering a client so it can notice peer shutdown. |
| `EPOLLHUP` | The connection has hung up. The server closes that client. |
| `EPOLLERR` | A socket error occurred. The server reads `SO_ERROR` with `getsockopt()` for the specific error, then closes the client. |

The server does not set `EPOLLET`, so it uses the default **level-triggered**
behavior. While a descriptor remains ready, epoll can report it again. The
server still drains `accept4()` and `recv()` until `EAGAIN`/`EWOULDBLOCK` so it
handles all currently available work efficiently.

### `epoll_wait()`

```cpp
epoll_wait(epoll_fd, events.data(), events.size(), 1000)
```

- `epoll_fd` identifies the event set to wait on.
- `events.data()` is the output array receiving ready events.
- `events.size()` is the maximum number of events returned in one call.
- `1000` is a timeout in milliseconds.

The one-second timeout lets the loop periodically check inactive-client
timeouts even when no descriptor becomes ready. The return value is the number
of events written to the array; `-1` indicates an error. `EINTR` is handled by
retrying the wait.

The event array has 128 entries. This is the maximum number of events returned
in a single call, not the maximum number of monitored clients. If more than 128
descriptors are ready, later calls return the remaining ready events.

## 5. Per-client state

The `clients` map associates each client file descriptor with a
`ClientConnection`. Each connection stores:

- `input`: request bytes accumulated from one or more `recv()` calls;
- `output`: serialized response bytes waiting to be sent;
- `output_offset`: the position of the next response byte to send;
- `response_ready`: whether the worker or event loop has prepared a response;
- `work_pending`: whether a worker is processing this client's request;
- `generation`: a unique connection identity used to reject stale worker
  completions;
- `last_activity`: the most recent client read/write activity, used for the
  idle timeout.

The event loop owns this map. Worker threads do not access it.

### Why the generation value matters

Operating systems may reuse a file descriptor after it is closed. A worker may
finish after its original client disconnected. If a new client then receives
the same descriptor number, using only the descriptor could attach the old
response to the new client.

Each accepted connection gets a new generation number. A completion is used
only when both the descriptor and generation match a live connection that is
still waiting for worker output.

## 6. Accepting clients

When `epoll_wait()` returns the listener's descriptor with `EPOLLIN`, the loop
marks the listener as ready and later calls `acceptClient()` repeatedly, up to
128 times for that batch.

For each accepted client:

1. The server creates an `epoll_event` watching `EPOLLIN | EPOLLRDHUP`.
2. `EPOLL_CTL_ADD` registers the client socket.
3. A new `ClientConnection` is inserted into the `clients` map.

If `accept4()` returns `EAGAIN`/`EWOULDBLOCK`, there are no more queued
connections to accept at that moment. The loop returns to `epoll_wait()`.

## 7. Reading and parsing an HTTP request

When a client reports `EPOLLIN`, the event loop calls nonblocking `recv()` in a
loop:

- A positive result means that many bytes were read; they are appended to the
  connection's `input` buffer.
- `0` means the peer has closed its write side; the current implementation
  closes the connection.
- `-1` with `EINTR` means retry the call.
- `-1` with `EAGAIN` or `EWOULDBLOCK` means there are no more bytes available
  now; return to epoll and wait for the next event.
- Other errors are logged and the connection is closed.

After appending bytes, `HttpParser::parse()` is called:

- `INCOMPLETE`: read more bytes when available.
- `BADREQUEST`: prepare a `400 Bad Request` response.
- `COMPLETE`: submit the parsed request to the worker pool.

The input buffer is capped by `MAX_REQUEST_SIZE`, currently about 1 MiB plus a
16 KiB header allowance and one read buffer. This protects the server from
unbounded request buffering.

## 8. Dispatching route work to the thread pool

The helper `workerCount()` chooses the number of worker threads:

- use `std::thread::hardware_concurrency()` when it reports a value;
- otherwise use 4;
- clamp the result to between 2 and 8 workers.

Once the request is complete, `submitRequest()`:

1. Checks the maximum queued-job count and queued request-byte limit.
2. Returns `503 Server Busy` if accepting more work would exceed either
   limit.
3. Marks the client as having pending work and removes its input buffer.
4. Disables normal socket events for this one-request connection while the
   handler is running.
5. Captures the request, router shared pointer, descriptor, generation, and
   request-byte count in a task submitted with `workers.push(...)`.

The maximums are currently:

- `MAX_PENDING_JOBS`: 1,024 running or queued tasks;
- `MAX_PENDING_REQUEST_BYTES`: 64 MiB of request bytes associated with those
  tasks.

These limits bound application work and memory use. Requests rejected at the
limit receive `503 Service Unavailable` with the body `Server Busy`.

### What happens in a worker

The worker calls `createResponse()`:

1. Creates an independent `HttpResponse`.
2. Calls `Router::dispatch()` to run middleware and the matching route handler.
3. Converts route-not-found, method-not-allowed, and middleware failures into
   suitable responses.
4. Catches handler exceptions and converts them into an HTTP 500 response.
5. Serializes the response into a string.
6. Places a `WorkerCompletion` in the shared completion queue.
7. Writes to the `eventfd` to wake the event loop.

The handler gets a request by value, so it does not refer to temporary
event-loop storage. The router is captured as a `std::shared_ptr` to keep it
alive until the task finishes.

`ctpl::thread_pool::push()` returns a `std::future`, but this server uses the
completion queue and `eventfd` to deliver the response. It does not wait on the
future from the event-loop thread; doing so would block the loop.

## 9. Synchronizing shared data

Different workers can call route handlers at the same time. This means route
handlers and middleware must treat shared mutable state as concurrent state.

For example, the REST example shares a mutable in-memory datastore. It protects
operations on that datastore with a `std::mutex` and `std::lock_guard` in
`examples/rest_api/main.cpp`.

In an application:

- protect shared mutable data with an appropriate mutex or use a
  thread-safe data structure;
- do not hold a lock while doing slow unrelated work;
- ensure middleware and route callbacks are safe to run concurrently;
- avoid modifying the router's routes or middleware after the server starts
  unless that operation is explicitly synchronized.

The server can move blocking route work off the network loop, but it cannot
make the handler's own shared data safe automatically.

## 10. Returning work to the event loop

The worker does not call `send()` on the client socket. Instead:

1. It locks `completed_jobs_mutex`.
2. It appends a `WorkerCompletion` to `completed_jobs`.
3. It unlocks the mutex.
4. It writes a 64-bit value to the nonblocking `eventfd`.

The event loop registered the `eventfd` with epoll for `EPOLLIN`. When notified,
it reads the eventfd counter, swaps the shared completion queue into a local
queue under the mutex, and processes each completion.

For each completion, the event loop:

- decrements the queued-job and queued-byte counters;
- checks that the client still exists, the generation matches, and work is
  still pending;
- stores the response in the connection state;
- changes its epoll interest to `EPOLLOUT`.

An `eventfd` is useful here because it is a Linux file descriptor: epoll can
monitor it just like a socket. Multiple worker writes can accumulate in the
eventfd counter, so one read notification can prompt the loop to drain several
queued completions.

## 11. Sending the response

When a client reports `EPOLLOUT`, the event loop sends from
`output + output_offset`:

- A positive `send()` result advances `output_offset`.
- `EINTR` retries the send.
- `EAGAIN` or `EWOULDBLOCK` means the kernel send buffer is full; the unsent
  response remains in the connection state until the next `EPOLLOUT`.
- `MSG_NOSIGNAL` prevents a send to a disconnected peer from raising
  `SIGPIPE`.

Once all response bytes have been sent, the server removes the descriptor from
epoll and closes it.

## 12. Closing idle or failed connections

The event loop closes clients on socket errors, a hangup, read/write failures,
or after the complete response is sent. It also checks `last_activity` after
each event batch and closes clients that have been inactive for 60 seconds.

Closing a client involves:

1. `epoll_ctl(..., EPOLL_CTL_DEL, ...)` to remove it from epoll;
2. `close()` on the socket;
3. erasing its per-connection state from the map.

If a worker finishes after this cleanup, the generation and map checks cause
the result to be discarded.

## 13. Linux and C++ APIs used

| API | Header | Purpose |
| --- | --- | --- |
| `socket()` | `<sys/socket.h>` | Create the TCP listening socket. |
| `setsockopt()` | `<sys/socket.h>` | Configure `SO_REUSEADDR`. |
| `bind()` | `<sys/socket.h>`, `<netinet/in.h>` | Assign the listening IP address and port. |
| `listen()` | `<sys/socket.h>` | Enable incoming connection requests. |
| `accept4()` | `<sys/socket.h>` | Accept a client and set its socket flags atomically. |
| `epoll_create1()` | `<sys/epoll.h>` | Create the Linux readiness event queue. |
| `epoll_ctl()` | `<sys/epoll.h>` | Add, modify, or remove monitored descriptors. |
| `epoll_wait()` | `<sys/epoll.h>` | Wait for descriptors to become ready. |
| `eventfd()` | `<sys/eventfd.h>` | Create a descriptor workers use to wake the event loop. |
| `recv()` | `<sys/socket.h>` | Read available request bytes from a client. |
| `send()` | `<sys/socket.h>` | Write available response bytes to a client. |
| `getsockopt()` | `<sys/socket.h>` | Read the pending socket error via `SO_ERROR`. |
| `close()` | `<unistd.h>` | Release socket, epoll, and eventfd descriptors. |
| `std::mutex` / `std::lock_guard` | `<mutex>` | Protect the worker completion queue and example datastore. |
| `std::thread::hardware_concurrency()` | `<thread>` | Suggest a worker count based on available CPU hardware. |
| `ctpl::thread_pool::push()` | `http_engine/ctpl_stl.h` | Submit a request handler task to a worker. |

## 14. Current behavior and limitations

- **One request per connection.** Every response includes `Connection: close`.
  Keep-alive and multiple requests on the same connection are not implemented.
- **No chunked request bodies.** The parser rejects `Transfer-Encoding`.
- **Handler concurrency is bounded.** The fixed pool has at most 8 workers, so
  slow handlers can occupy all workers. The bounded queue then eventually
  returns 503 responses.
- **Application thread safety is required.** Shared mutable state in handlers
  needs synchronization.
- **Some work remains on the event-loop thread.** Accepting, reading, parsing,
  response sends, logging in the network path, and timeout scans are performed
  by the loop. Keep these operations short.
- **Static file middleware may block.** If it runs as a handler and reads files
  synchronously, it now runs on a worker, not the event loop, but it still
  consumes a worker while it performs disk I/O.
- **The idle timeout is not a complete slow-request defense.** It measures
  time since activity, so a client that periodically sends bytes may keep a
  request open. A total request deadline or minimum receive rate can be added
  separately.
- **File descriptor limits still apply.** Check the process's `RLIMIT_NOFILE`
  when testing large active-connection counts. The listen backlog and worker
  count do not raise that limit.
- **This is not a benchmark guarantee.** Actual capacity depends on request
  sizes, route work, response sizes, CPU, memory, file-descriptor limits, and
  operating-system configuration.

## 15. Build and run

From the repository root:

```bash
cmake --build build -j2
./references/hello_server
```

The REST example can be started with:

```bash
./references/rest_api
```

Both examples listen on port 8080, so run only one at a time. Test a route with:

```bash
curl -i http://127.0.0.1:8080/hello
```

For a concurrency test, use a load-testing tool or a client that opens many
connections concurrently. Check both successful responses and 503 responses:
503 is the intentional overload behavior when bounded work limits are reached.

## 16. Request lifecycle summary

1. `socket()` creates a nonblocking listener.
2. `bind()` assigns its address; `listen()` starts accepting connections.
3. `epoll_create1()` creates an event queue.
4. `epoll_ctl(ADD)` registers the listener and worker `eventfd`.
5. `epoll_wait()` reports listener, client, and worker-completion readiness.
6. `accept4()` accepts clients; epoll registers each client for `EPOLLIN`.
7. `recv()` accumulates bytes until the HTTP parser has a complete request.
8. The bounded pool queue receives the request; a worker runs middleware,
   routing, and response serialization.
9. The worker puts the result in a mutex-protected queue and signals `eventfd`.
10. Epoll wakes the event loop, which retrieves the result and registers the
    client for `EPOLLOUT`.
11. `send()` writes the response, preserving the offset if only part can be
    sent.
12. The event loop removes and closes the client socket.
