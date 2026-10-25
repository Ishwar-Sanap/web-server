#include "http_engine/server.hpp"
#include "http_engine/ctpl_stl.h"
#include "http_engine/http_parser.hpp"
#include "http_engine/response_serializer.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iostream>
#include <mutex>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <utility>

namespace http_engine
{
    namespace
    {
        constexpr std::size_t READ_BUFFER_SIZE = 4096;
        constexpr std::size_t MAX_REQUEST_SIZE = 16 * 1024 + 1024 * 1024 + READ_BUFFER_SIZE;
        constexpr auto CLIENT_IDLE_TIMEOUT = std::chrono::seconds(60);
        // Bound queued work by both job count and request bytes to control memory use.
        constexpr std::size_t MAX_PENDING_JOBS = 1024;
        constexpr std::size_t MAX_PENDING_REQUEST_BYTES = 64 * 1024 * 1024;

        struct ClientConnection
        {
            // Keep request/response progress per socket while other clients are serviced.
            std::string input;
            std::string output;
            // Offset lets us resume a response if send() writes only part of it.
            std::size_t output_offset = 0;
            bool response_ready = false;
            bool work_pending = false;
            std::uint64_t generation = 0;
            std::chrono::steady_clock::time_point last_activity =
                std::chrono::steady_clock::now();
        };

        struct WorkerCompletion
        {
            int client_fd;
            std::uint64_t generation;
            std::size_t request_bytes;
            std::string response;
        };

        int workerCount()
        {
            const unsigned int available = std::thread::hardware_concurrency();
            const unsigned int count = available == 0 ? 4 : available;
            return static_cast<int>(std::min(8u, std::max(2u, count)));
        }

        std::string createResponse(const std::shared_ptr<web_core::Router> &router, HttpRequest request)
        {
            HttpResponse response;
            response.setHeader("Connection", "close");

            if (!router)
            {
                response.status(500).send("Internal Server Error");
            }
            else
            {
                try
                {
                    //Dispath request to router
                    const web_core::Router::DispatchResult result = router->dispatch(request, response);

                    if (result.status == web_core::Router::DispatchResult::Status::ROUTE_NOT_FOUND)
                    {
                        response.status(404).send("Cannot " + request.method() + " " + request.path());
                    }
                    else if (result.status == web_core::Router::DispatchResult::Status::METHOD_NOT_ALLOWED)
                    {
                        std::string allowed_methods;
                        for (const std::string &method : result.allowed_methods)
                        {
                            if (!allowed_methods.empty())
                            {
                                allowed_methods += ", ";
                            }
                            allowed_methods += method;
                        }

                        response.setHeader("Allow", allowed_methods).status(405).send("Method Not Allowed");
                    }
                    else if (result.status == web_core::Router::DispatchResult::Status::MIDDLEWARE_NOT_SENT_RESP)
                    {
                        response.status(500).send("Internal Server Error: middleware did not call next() or send a response.");
                    }
                }
                catch (const std::exception &error)
                {
                    std::cerr << "Request handler failed: " << error.what() << "\n";
                    response = HttpResponse{};
                    response.setHeader("Connection", "close")
                        .status(500)
                        .send("Internal Server Error");
                }
                catch (...)
                {
                    std::cerr << "Request handler failed with an unknown exception\n";
                    response = HttpResponse{};
                    response.setHeader("Connection", "close")
                        .status(500)
                        .send("Internal Server Error");
                }
            }

            ResponseSerializer serializer;
            try
            {
                return serializer.serialize(response);
            }
            catch (const std::exception &error)
            {
                std::cerr << "Response serialization failed: " << error.what() << "\n";
                HttpResponse fallback;
                fallback.setHeader("Connection", "close")
                    .status(500)
                    .send("Internal Server Error");
                return serializer.serialize(fallback);
            }
        }
    }

    Server::Server(int port) : m_listener(port)
    {
    }

    void Server::setRouter(std::shared_ptr<web_core::Router> router)
    {
        m_router = std::move(router);
    }

    void Server::run()
    {
        if (!m_listener.start())
        {
            std::cerr << "Failed to start server\n";
            return;
        }

        // Create the kernel event queue; CLOEXEC prevents child processes from inheriting it.
        const int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd == -1)
        {
            std::cerr << "epoll_create1() failed: " << std::strerror(errno) << "\n";
            return;
        }

        epoll_event listener_event{};
        // EPOLLIN asks epoll to notify us when the listening socket can accept clients.
        listener_event.events = EPOLLIN;
        // epoll_wait() returns this fd in data.fd so we can distinguish the listener from clients.
        listener_event.data.fd = m_listener.fileDescriptor();

        // ADD registers the listener with this epoll instance.
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, m_listener.fileDescriptor(), &listener_event) == -1)
        {
            std::cerr << "epoll_ctl() failed for listener: " << std::strerror(errno) << "\n";
            close(epoll_fd);
            return;
        }

        const int completion_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

        if (completion_fd == -1)
        {
            std::cerr << "eventfd() failed: " << std::strerror(errno) << "\n";
            close(epoll_fd);
            return;
        }

        epoll_event completion_event{};
        completion_event.events = EPOLLIN;
        completion_event.data.fd = completion_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, completion_fd, &completion_event) == -1)
        {
            std::cerr << "epoll_ctl() failed for completion event: "
                      << std::strerror(errno) << "\n";
            close(completion_fd);
            close(epoll_fd);
            return;
        }

        std::unordered_map<int, ClientConnection> clients;
        std::deque<WorkerCompletion> completed_jobs;
        std::mutex completed_jobs_mutex;
        ctpl::thread_pool workers(workerCount());
        std::size_t pending_jobs = 0;
        std::size_t pending_request_bytes = 0;
        std::uint64_t next_generation = 1;
        HttpParser parser;
        ResponseSerializer serializer;

        auto closeClient = [&](int client_fd)
        {
            // epoll_ctl takes the epoll fd, operation, target socket fd, and event settings.
            // DEL removes the socket; it does not need event settings, so the last argument is null.
            if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr) == -1 &&
                errno != ENOENT && errno != EBADF)
            {
                std::cerr << "epoll_ctl() failed while removing client: "
                          << std::strerror(errno) << "\n";
            }

            close(client_fd);
            clients.erase(client_fd);
        };

        auto setClientEvents = [&](int client_fd, std::uint32_t events)
        {
            epoll_event event{};
            // MOD replaces the watched events; data.fd identifies this client in epoll_wait().
            event.events = events;
            event.data.fd = client_fd;

            // MOD switches this client to EPOLLOUT after a response is ready to send.
            if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &event) == -1)
            {
                std::cerr << "epoll_ctl() failed while updating client: "
                          << std::strerror(errno) << "\n";
                return false;
            }

            return true;
        };

        auto queueResponse = [&](int client_fd, ClientConnection &client,
                                 int status, const std::string &body)
        {
            HttpResponse response;
            response.setHeader("Connection", "close").status(status).send(body);

            try
            {
                client.output = serializer.serialize(response);
            }
            catch (const std::exception &error)
            {
                std::cerr << "Response serialization failed: " << error.what() << "\n";
                HttpResponse fallback;
                fallback.setHeader("Connection", "close")
                    .status(500)
                    .send("Internal Server Error");
                client.output = serializer.serialize(fallback);
            }

            client.input = std::string{};
            client.output_offset = 0;
            client.response_ready = true;
            return setClientEvents(client_fd, EPOLLOUT);
        };

        auto submitRequest = [&](int client_fd, ClientConnection &client, HttpRequest request)
        {
            const std::size_t request_bytes = client.input.size();
            if (pending_jobs >= MAX_PENDING_JOBS ||
                request_bytes > MAX_PENDING_REQUEST_BYTES - pending_request_bytes)
            {
                return queueResponse(client_fd, client, 503, "Server Busy");
            }

            client.work_pending = true;
            client.input.clear();
            client.last_activity = std::chrono::steady_clock::now();
            ++pending_jobs;
            pending_request_bytes += request_bytes;

            // Stop reading this one-request connection while its handler runs.
            if (!setClientEvents(client_fd, 0))
            {
                client.work_pending = false;
                --pending_jobs;
                pending_request_bytes -= request_bytes;
                return false;
            }

            const std::uint64_t generation = client.generation;
            const std::shared_ptr<web_core::Router> router = m_router;
            try
            {
                // Storing callback function to workers, so it can be called by worker thread later

                workers.push([&, client_fd, generation, request_bytes, router, request = std::move(request)](int) mutable
                             {
                   
                    WorkerCompletion completion{client_fd, generation, request_bytes, createResponse(router, std::move(request)) };

                    {
                        std::lock_guard<std::mutex> lock(completed_jobs_mutex);
                        completed_jobs.push_back(std::move(completion));
                    }

                    //Write into completion_fd, will notify that for one of worker thread taks is completed
                    const std::uint64_t signal_value = 1;
                    while (write(completion_fd, &signal_value, sizeof(signal_value)) == -1)
                    {
                        if (errno == EINTR)
                        {
                            continue;
                        }
                        if (errno != EAGAIN && errno != EWOULDBLOCK)
                        {
                            std::cerr << "eventfd write failed: "
                                      << std::strerror(errno) << "\n";
                        }
                        break;
                    } });
            }
            catch (const std::exception &error)
            {
                std::cerr << "Could not queue request handler: " << error.what() << "\n";
                client.work_pending = false;
                --pending_jobs;
                pending_request_bytes -= request_bytes;
                return queueResponse(client_fd, client, 503, "Server Busy");
            }

            return true;
        };

        // epoll_wait() can return readiness information for at most 128 sockets per call.
        std::array<epoll_event, 128> events{};

        while (true)
        {
            // Wait on epoll_fd, store ready events in events, and wait at most 1 second.
            // The timeout also lets us check idle-client timeouts.
            // The return value is the number of ready events stored in events.
            const int event_count = epoll_wait(epoll_fd, events.data(), static_cast<int>(events.size()), 1000);

            if (event_count == -1)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                std::cerr << "epoll_wait() failed: " << std::strerror(errno) << "\n";
                break;
            }

            bool listener_ready = false;

            for (int i = 0; i < event_count; ++i)
            {
                // data.fd is the descriptor saved when the socket was registered with epoll.
                const int ready_fd = events[static_cast<std::size_t>(i)].data.fd;
                // This bitmask reports which readiness or error conditions occurred.
                const std::uint32_t ready_events = events[static_cast<std::size_t>(i)].events;

                if (ready_fd == completion_fd)
                {
                    // Read the eventfd counter to clear its readable state.
                    std::uint64_t signal_value = 0;
                    while (read(completion_fd, &signal_value, sizeof(signal_value)) == -1)
                    {
                        if (errno == EINTR)
                        {
                            continue;
                        }
                        if (errno != EAGAIN && errno != EWOULDBLOCK)
                        {
                            std::cerr << "eventfd read failed: "
                                      << std::strerror(errno) << "\n";
                        }
                        break;
                    }

                    std::deque<WorkerCompletion> ready_completions;
                    {
                        std::lock_guard<std::mutex> lock(completed_jobs_mutex);
                        ready_completions.swap(completed_jobs); // here we are modifying the shared deque (this deque is also accesed by worker thread) so that's why used  lock_guard 

                        /*
                        After swap:
                            ready_completions holds the items workers had queued.
                            completed_jobs is empty and ready for workers to add more results.
                            The swap happens while holding the mutex, so it takes only a short time. The event loop then processes ready_completions after unlocking, letting workers continue adding results without waiting for response processing to finish.
                        */
                    }

                    for (WorkerCompletion &completion : ready_completions)
                    {
                        --pending_jobs;
                        pending_request_bytes -= completion.request_bytes;
                        auto client_position = clients.find(completion.client_fd);
                        if (client_position == clients.end() ||
                            client_position->second.generation != completion.generation ||
                            !client_position->second.work_pending)
                        {
                            continue;
                        }

                        ClientConnection &client = client_position->second;
                        client.work_pending = false;
                        client.output = std::move(completion.response);
                        client.output_offset = 0;
                        client.response_ready = true;
                        client.last_activity = std::chrono::steady_clock::now();

                        if (!setClientEvents(completion.client_fd, EPOLLOUT))
                        {
                            closeClient(completion.client_fd);
                        }
                    }
                    continue;
                }

                if (ready_fd == m_listener.fileDescriptor())
                {
                    // Process accepts after other ready clients in this event batch.
                    listener_ready = true;
                    continue;
                }

                auto client_position = clients.find(ready_fd);
                if (client_position == clients.end())
                {
                    continue;
                }

                ClientConnection &client = client_position->second;

                // EPOLLERR signals a socket error; SO_ERROR retrieves its specific error code.
                if (ready_events & EPOLLERR)
                {
                    int socket_error = 0;
                    socklen_t error_size = sizeof(socket_error);
                    if (getsockopt(ready_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_size) == 0 && socket_error != 0)
                    {
                        std::cerr << "Client socket error: " << std::strerror(socket_error) << "\n";
                    }
                    closeClient(ready_fd);
                    continue;
                }

                bool close_connection = false;
                // EPOLLIN means data can be read; RDHUP/HUP can also indicate peer shutdown.
                if (!client.response_ready && !client.work_pending && (ready_events & (EPOLLIN | EPOLLRDHUP | EPOLLHUP)))
                {
                    std::array<char, READ_BUFFER_SIZE> buffer{};

                    while (true)
                    {
                        if (client.input.size() >= MAX_REQUEST_SIZE)
                        {
                            if (!queueResponse(ready_fd, client, 400, "Bad Request"))
                            {
                                close_connection = true;
                            }
                            break;
                        }

                        const std::size_t available = MAX_REQUEST_SIZE - client.input.size();
                        const std::size_t read_size = std::min(buffer.size(), available);

                        //recieve the request data and store into buffer
                        // Drain available data until EAGAIN; nonblocking recv() never waits.
                        const ssize_t bytes_read = recv(ready_fd, buffer.data(), read_size, 0);

                        if (bytes_read > 0)
                        {
                            client.last_activity = std::chrono::steady_clock::now();
                            client.input.append(buffer.data(), static_cast<std::size_t>(bytes_read));

                            HttpRequest request;
                            std::size_t bytes_consumed = 0;
                            //parse raw input into HttpRequest format
                            const ParseResult result = parser.parse(client.input, request, bytes_consumed);

                            if (result == ParseResult::COMPLETE)
                            {
                                if (!submitRequest(ready_fd, client, std::move(request)))
                                {
                                    close_connection = true;
                                }
                                break;
                            }

                            if (result == ParseResult::BADREQUEST)
                            {
                                if (!queueResponse(ready_fd, client, 400, "Bad Request"))
                                {
                                    close_connection = true;
                                }
                                break;
                            }

                            continue;
                        }

                        if (bytes_read == 0)
                        {
                            close_connection = true;
                            break;
                        }

                        if (errno == EINTR)
                        {
                            continue;
                        }

                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            // No more bytes are ready now; epoll will notify us when they arrive.
                            break;
                        }

                        std::cerr << "recv() failed: " << std::strerror(errno) << "\n";
                        close_connection = true;
                        break;
                    }
                }

                // EPOLLHUP means the connection has hung up and should be closed.
                if (ready_events & EPOLLHUP)
                {
                    close_connection = true;
                }

                // EPOLLOUT means the socket can accept data; output_offset tracks partial sends.
                if (!close_connection && client.response_ready && (ready_events & EPOLLOUT))
                {
                    // sending the response back to client
                    while (client.output_offset < client.output.size())
                    {
                        const ssize_t bytes_sent =
                            send(ready_fd, client.output.data() + client.output_offset, client.output.size() - client.output_offset, MSG_NOSIGNAL);

                        if (bytes_sent > 0)
                        {
                            client.last_activity = std::chrono::steady_clock::now();
                            client.output_offset += static_cast<std::size_t>(bytes_sent);
                            continue;
                        }

                        if (bytes_sent == -1 && errno == EINTR)
                        {
                            continue;
                        }

                        if (bytes_sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
                        {
                            // The send buffer is full; wait for another EPOLLOUT notification.
                            break;
                        }

                        std::cerr << "send() failed: " << std::strerror(errno) << "\n";
                        close_connection = true;
                        break;
                    }

                    // all response send to client
                    if (client.output_offset == client.output.size())
                    {
                        close_connection = true;
                    }
                }

                if (close_connection)
                {
                    closeClient(ready_fd);
                }
            }

            const auto now = std::chrono::steady_clock::now();

            for (auto client_position = clients.begin(); client_position != clients.end();)
            {
                const int client_fd = client_position->first;
                if (now - client_position->second.last_activity > CLIENT_IDLE_TIMEOUT)
                {
                    ++client_position;
                    closeClient(client_fd);
                }
                else
                {
                    ++client_position;
                }
            }

            // Accepting new clients
            if (listener_ready)
            {
                for (int accepted = 0; accepted < 128; ++accepted)
                {
                    const int client_fd = m_listener.acceptClient();
                    if (client_fd == -1)
                    {
                        if (errno == EINTR)
                        {
                            --accepted;
                            continue;
                        }
                        break;
                    }

                    epoll_event client_event{};
                    // Watch new clients for request data and for a peer write-side shutdown.
                    client_event.events = EPOLLIN | EPOLLRDHUP;
                    // Store the fd in returned events so the matching client state can be found.
                    client_event.data.fd = client_fd;

                    // ADD begins monitoring this newly accepted socket.
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) == -1)
                    {
                        std::cerr << "epoll_ctl() failed while adding client: " << std::strerror(errno) << "\n";
                        close(client_fd);
                        continue;
                    }

                    ClientConnection client;
                    client.generation = next_generation++;
                    clients.emplace(client_fd, std::move(client));
                }
            }
        }

        for (const auto &[client_fd, client] : clients)
        {
            (void)client;
            close(client_fd);
        }
        workers.stop(true);
        close(completion_fd);
        close(epoll_fd);
    }
}
