# Local HTTP Server Benchmark

## Run configuration

| Metric | Value |
| --- | --- |
| Timestamp (UTC) | 2026-10-25T18:36:06.020732+00:00 |
| Endpoint | `http://127.0.0.1:8080/users` |
| Resolved loopback addresses | 127.0.0.1 |
| Requests requested | 10000 |
| Client concurrency limit | 1000 |
| Client workers used | 1000 |
| Per-request timeout | 10.0 s |
| Total elapsed | 3.013 s |
| Python | 3.10.12 |
| Platform | Linux-6.6.87.2-microsoft-standard-WSL2-x86_64-with-glibc2.35 |
| CPU count reported by client | 8 |

## Throughput and completion

| Metric | Result |
| --- | ---: |
| HTTP responses received | 10000 / 10000 |
| Successful 2xx responses | 10000 |
| Non-2xx HTTP responses | 0 |
| Failed requests (no valid HTTP response) | 0 |
| 2xx success rate | 100.00% |
| Valid HTTP response rate | 100.00% |
| Attempt throughput | 3318.80 requests/s |
| Valid response throughput | 3318.80 responses/s |
| Successful throughput | 3318.80 responses/s |
| Peak simultaneous request tasks | 1000 |
| Peak open client connections | 1000 |

## Latency

Latency includes TCP connection establishment, request transmission, and
waiting for the server to close the response connection. Percentiles are
calculated over completed valid HTTP responses; the successful-response row
excludes non-2xx statuses.

| Population | Summary |
| --- | --- |
| All valid HTTP responses | n=10000, mean=275.93 ms, p50=242.73 ms, p90=376.19 ms, p95=445.32 ms, p99=448.79 ms, max=449.13 ms |
| Successful 2xx responses | n=10000, mean=275.93 ms, p50=242.73 ms, p90=376.19 ms, p95=445.32 ms, p99=448.79 ms, max=449.13 ms |
| TCP connect time | n=10000, mean=201.07 ms, p50=181.80 ms, p90=282.90 ms, p95=296.32 ms, p99=325.83 ms, max=360.28 ms |

## HTTP status codes

| Status | Count |
| --- | ---: |
| 200 | 10000 |

## Client-side errors

| Error | Count |
| --- | ---: |
| None | 0 |

## Data volume and client resource use

| Metric | Result |
| --- | ---: |
| Request bytes per attempt | 64 bytes |
| Request bytes sent (estimated) | 640000 bytes |
| Response bytes received | 3140000 bytes (2.99 MiB) |
| Benchmark client CPU time | 3.053 s |
| Benchmark client CPU use | 101.3% of one core |
| Benchmark client peak RSS | 34048 KiB |

## Interpretation notes

- This is a client-side benchmark against a loopback server. It measures
  end-to-end local request latency and throughput, not Internet/network latency.
- Client CPU and RSS describe the Python benchmark process, **not the server**.
  Measure the server separately (for example with `pidstat`, `top`, or
  `/usr/bin/time -v`) if reporting server CPU or memory.
- The server handles one request per TCP connection and closes after each
  response. This script uses one connection per request to match that behavior.
- HTTP 503 responses are valid HTTP responses but are not successful 2xx
  responses. They may indicate that the server's bounded worker queue was full.
- State the exact command, route, machine, build type, and run configuration
  when presenting results. A benchmark result is not meaningful without them.
