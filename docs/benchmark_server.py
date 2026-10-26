#!/usr/bin/env python3
"""Benchmark a local HTTP server using concurrent, one-request TCP connections."""

import argparse
import asyncio
from collections import Counter
from datetime import datetime, timezone
import ipaddress
import json
import math
import os
import platform
import resource
import socket
import sys
import time
from urllib.parse import urlsplit


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Measure local server latency and throughput. "
            "Only loopback destinations are allowed."
        )
    )
    parser.add_argument("--host", default="127.0.0.1", help="Loopback server host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=8080, help="Server port (default: 8080)")
    parser.add_argument("--path", default="/hello", help="HTTP request path (default: /hello)")
    parser.add_argument("--requests", type=int, default=10000, help="Total measured requests (default: 10000)")
    parser.add_argument(
        "--concurrency", type=int, default=1000,
        help="Maximum simultaneous client connections (default: 1000)",
    )
    parser.add_argument("--timeout", type=float, default=10.0, help="Per-request timeout in seconds")
    parser.add_argument("--report", help="Write a Markdown report to this path")
    parser.add_argument("--json", dest="json_path", help="Write machine-readable JSON results to this path")
    args = parser.parse_args()

    if not 1 <= args.port <= 65535:
        parser.error("--port must be between 1 and 65535")
    if args.requests < 1:
        parser.error("--requests must be at least 1")
    if args.concurrency < 1:
        parser.error("--concurrency must be at least 1")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    if not args.path.startswith("/") or "\r" in args.path or "\n" in args.path:
        parser.error("--path must begin with / and must not contain newlines")
    if any(char in args.host for char in "\r\n"):
        parser.error("--host must not contain newlines")
    return args


async def require_loopback(host, port):
    loop = asyncio.get_running_loop()
    try:
        addresses = await loop.getaddrinfo(
            host, port, type=socket.SOCK_STREAM, proto=socket.IPPROTO_TCP
        )
    except OSError as error:
        raise ValueError(f"Could not resolve {host}: {error}") from error

    if not addresses:
        raise ValueError(f"No addresses found for {host}")

    resolved = {address[4][0] for address in addresses}
    if any(not ipaddress.ip_address(address).is_loopback for address in resolved):
        raise ValueError(
            f"Refusing non-loopback host {host!r} ({', '.join(sorted(resolved))}). "
            "This benchmark is restricted to localhost."
        )
    return sorted(resolved)


def parse_response(response):
    header_bytes, separator, body = response.partition(b"\r\n\r\n")
    if not separator:
        raise ValueError("response ended before the HTTP headers were complete")

    lines = header_bytes.split(b"\r\n")
    if not lines:
        raise ValueError("response is missing a status line")

    parts = lines[0].split()
    if len(parts) < 2 or not parts[0].startswith(b"HTTP/"):
        raise ValueError("response has an invalid HTTP status line")

    try:
        status = int(parts[1])
    except ValueError as error:
        raise ValueError("response has a non-numeric HTTP status") from error

    content_length = None
    for line in lines[1:]:
        name, colon, value = line.partition(b":")
        if not colon:
            raise ValueError("response contains a malformed HTTP header")
        if name.strip().lower() == b"content-length":
            try:
                content_length = int(value.strip())
            except ValueError as error:
                raise ValueError("response has an invalid Content-Length") from error

    if content_length is not None and content_length != len(body):
        raise ValueError(
            f"Content-Length says {content_length} bytes but received {len(body)}"
        )
    return status, len(body)


def percentile(sorted_values, percent):
    if not sorted_values:
        return None
    index = (len(sorted_values) - 1) * percent / 100
    lower = math.floor(index)
    upper = math.ceil(index)
    if lower == upper:
        return sorted_values[lower]
    fraction = index - lower
    return sorted_values[lower] * (1 - fraction) + sorted_values[upper] * fraction


def summarize_latencies(values):
    if not values:
        return None
    ordered = sorted(values)
    return {
        "count": len(ordered),
        "mean_ms": math.fsum(ordered) / len(ordered),
        "p50_ms": percentile(ordered, 50),
        "p90_ms": percentile(ordered, 90),
        "p95_ms": percentile(ordered, 95),
        "p99_ms": percentile(ordered, 99),
        "max_ms": ordered[-1],
    }


async def run_benchmark(args, resolved_addresses):
    request_bytes = (
        f"GET {args.path} HTTP/1.1\r\n"
        f"Host: {args.host}:{args.port}\r\n"
        "Connection: close\r\n"
        "\r\n"
    ).encode("ascii")

    status_counts = Counter()
    error_counts = Counter()
    all_latencies = []
    successful_latencies = []
    connect_latencies = []
    response_bytes = 0
    completed_http_responses = 0
    active_requests = 0
    peak_active_requests = 0
    active_connections = 0
    peak_open_connections = 0
    next_request = 0
    client_cpu_start = resource.getrusage(resource.RUSAGE_SELF).ru_utime + resource.getrusage(
        resource.RUSAGE_SELF
    ).ru_stime
    started_at = time.perf_counter()

    async def one_request():
        nonlocal response_bytes, completed_http_responses
        nonlocal active_requests, peak_active_requests
        nonlocal active_connections, peak_open_connections

        active_requests += 1
        peak_active_requests = max(peak_active_requests, active_requests)
        request_started = time.perf_counter()
        writer = None
        connected = False
        try:
            deadline = request_started + args.timeout
            remaining = deadline - time.perf_counter()
            if remaining <= 0:
                raise asyncio.TimeoutError()

            connect_started = time.perf_counter()
            reader, writer = await asyncio.wait_for(
                asyncio.open_connection(args.host, args.port), timeout=remaining
            )
            connect_latencies.append((time.perf_counter() - connect_started) * 1000)
            connected = True
            active_connections += 1
            peak_open_connections = max(peak_open_connections, active_connections)

            writer.write(request_bytes)
            remaining = deadline - time.perf_counter()
            if remaining <= 0:
                raise asyncio.TimeoutError()
            await asyncio.wait_for(writer.drain(), timeout=remaining)

            remaining = deadline - time.perf_counter()
            if remaining <= 0:
                raise asyncio.TimeoutError()
            response = await asyncio.wait_for(reader.read(), timeout=remaining)
            elapsed_ms = (time.perf_counter() - request_started) * 1000
            status, body_size = parse_response(response)

            completed_http_responses += 1
            response_bytes += len(response)
            status_counts[str(status)] += 1
            all_latencies.append(elapsed_ms)
            if 200 <= status < 300:
                successful_latencies.append(elapsed_ms)
        except asyncio.TimeoutError:
            error_counts["timeout"] += 1
        except (ConnectionError, OSError) as error:
            error_counts[f"connection: {type(error).__name__}"] += 1
        except ValueError as error:
            error_counts[f"invalid_response: {error}"] += 1
        except Exception as error:
            error_counts[f"other: {type(error).__name__}: {error}"] += 1
        finally:
            if writer is not None:
                writer.close()
                try:
                    await writer.wait_closed()
                except (ConnectionError, OSError):
                    pass
            if connected:
                active_connections -= 1
            active_requests -= 1

    async def worker():
        nonlocal next_request
        while next_request < args.requests:
            next_request += 1
            await one_request()

    worker_count = min(args.requests, args.concurrency)
    await asyncio.gather(*(worker() for _ in range(worker_count)))
    elapsed_seconds = time.perf_counter() - started_at
    usage = resource.getrusage(resource.RUSAGE_SELF)
    client_cpu_seconds = usage.ru_utime + usage.ru_stime - client_cpu_start

    statuses = {int(status): count for status, count in sorted(status_counts.items(), key=lambda item: int(item[0]))}
    successful_count = sum(count for status, count in statuses.items() if 200 <= status < 300)
    non_2xx_count = completed_http_responses - successful_count
    result = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "host": args.host,
        "resolved_addresses": resolved_addresses,
        "port": args.port,
        "path": args.path,
        "requests_requested": args.requests,
        "concurrency_limit": args.concurrency,
        "workers_used_by_client": worker_count,
        "timeout_seconds": args.timeout,
        "elapsed_seconds": elapsed_seconds,
        "completed_http_responses": completed_http_responses,
        "successful_2xx_responses": successful_count,
        "non_2xx_responses": non_2xx_count,
        "failed_requests": args.requests - completed_http_responses,
        "success_rate_percent": successful_count / args.requests * 100,
        "http_response_rate_percent": completed_http_responses / args.requests * 100,
        "requests_per_second": args.requests / elapsed_seconds,
        "http_responses_per_second": completed_http_responses / elapsed_seconds,
        "successful_responses_per_second": successful_count / elapsed_seconds,
        "peak_active_requests": peak_active_requests,
        "peak_open_connections": peak_open_connections,
        "status_counts": statuses,
        "error_counts": dict(error_counts),
        "latency_all_http_responses": summarize_latencies(all_latencies),
        "latency_successful_2xx": summarize_latencies(successful_latencies),
        "connect_latency": summarize_latencies(connect_latencies),
        "request_bytes_each": len(request_bytes),
        "request_bytes_total": len(request_bytes) * args.requests,
        "response_bytes_total": response_bytes,
        "response_mib_total": response_bytes / (1024 * 1024),
        "client_cpu_seconds": client_cpu_seconds,
        "client_cpu_percent_of_one_core": client_cpu_seconds / elapsed_seconds * 100,
        "client_peak_rss_kib": usage.ru_maxrss,
        "python_version": sys.version.split()[0],
        "platform": platform.platform(),
        "cpu_count_reported": os.cpu_count(),
    }
    return result


def format_latency(summary):
    if summary is None:
        return "No valid HTTP responses"
    return (
        f"n={summary['count']}, mean={summary['mean_ms']:.2f} ms, "
        f"p50={summary['p50_ms']:.2f} ms, p90={summary['p90_ms']:.2f} ms, "
        f"p95={summary['p95_ms']:.2f} ms, p99={summary['p99_ms']:.2f} ms, "
        f"max={summary['max_ms']:.2f} ms"
    )


def create_markdown_report(result):
    status_rows = "\n".join(
        f"| {status} | {count} |" for status, count in result["status_counts"].items()
    ) or "| — | 0 |"
    error_rows = "\n".join(
        f"| {error} | {count} |" for error, count in result["error_counts"].items()
    ) or "| None | 0 |"

    return f"""# Local HTTP Server Benchmark

## Run configuration

| Metric | Value |
| --- | --- |
| Timestamp (UTC) | {result['timestamp_utc']} |
| Endpoint | `http://{result['host']}:{result['port']}{result['path']}` |
| Resolved loopback addresses | {', '.join(result['resolved_addresses'])} |
| Requests requested | {result['requests_requested']} |
| Client concurrency limit | {result['concurrency_limit']} |
| Client workers used | {result['workers_used_by_client']} |
| Per-request timeout | {result['timeout_seconds']} s |
| Total elapsed | {result['elapsed_seconds']:.3f} s |
| Python | {result['python_version']} |
| Platform | {result['platform']} |
| CPU count reported by client | {result['cpu_count_reported']} |

## Throughput and completion

| Metric | Result |
| --- | ---: |
| HTTP responses received | {result['completed_http_responses']} / {result['requests_requested']} |
| Successful 2xx responses | {result['successful_2xx_responses']} |
| Non-2xx HTTP responses | {result['non_2xx_responses']} |
| Failed requests (no valid HTTP response) | {result['failed_requests']} |
| 2xx success rate | {result['success_rate_percent']:.2f}% |
| Valid HTTP response rate | {result['http_response_rate_percent']:.2f}% |
| Attempt throughput | {result['requests_per_second']:.2f} requests/s |
| Valid response throughput | {result['http_responses_per_second']:.2f} responses/s |
| Successful throughput | {result['successful_responses_per_second']:.2f} responses/s |
| Peak simultaneous request tasks | {result['peak_active_requests']} |
| Peak open client connections | {result['peak_open_connections']} |

## Latency

Latency includes TCP connection establishment, request transmission, and
waiting for the server to close the response connection. Percentiles are
calculated over completed valid HTTP responses; the successful-response row
excludes non-2xx statuses.

| Population | Summary |
| --- | --- |
| All valid HTTP responses | {format_latency(result['latency_all_http_responses'])} |
| Successful 2xx responses | {format_latency(result['latency_successful_2xx'])} |
| TCP connect time | {format_latency(result['connect_latency'])} |

## HTTP status codes

| Status | Count |
| --- | ---: |
{status_rows}

## Client-side errors

| Error | Count |
| --- | ---: |
{error_rows}

## Data volume and client resource use

| Metric | Result |
| --- | ---: |
| Request bytes per attempt | {result['request_bytes_each']} bytes |
| Request bytes sent (estimated) | {result['request_bytes_total']} bytes |
| Response bytes received | {result['response_bytes_total']} bytes ({result['response_mib_total']:.2f} MiB) |
| Benchmark client CPU time | {result['client_cpu_seconds']:.3f} s |
| Benchmark client CPU use | {result['client_cpu_percent_of_one_core']:.1f}% of one core |
| Benchmark client peak RSS | {result['client_peak_rss_kib']} KiB |

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
"""


async def main():
    args = parse_args()
    try:
        resolved_addresses = await require_loopback(args.host, args.port)
    except ValueError as error:
        print(f"Error: {error}", file=sys.stderr)
        return 2

    print(
        f"Benchmarking http://{args.host}:{args.port}{args.path}: "
        f"{args.requests} requests, up to {args.concurrency} concurrent connections"
    )
    result = await run_benchmark(args, resolved_addresses)
    report = create_markdown_report(result)
    print(report)

    if args.report:
        with open(args.report, "w", encoding="utf-8") as report_file:
            report_file.write(report)
        print(f"Markdown report written to {args.report}")

    if args.json_path:
        with open(args.json_path, "w", encoding="utf-8") as json_file:
            json.dump(result, json_file, indent=2)
            json_file.write("\n")
        print(f"JSON results written to {args.json_path}")

    return 0 if result["failed_requests"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
