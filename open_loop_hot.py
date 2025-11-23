#!/usr/bin/env python3
"""
Simple open-loop hot-cache load generator (no think time, many short-lived connections).
Use when you want to push server CPU harder than wrk's closed-loop pattern.

Example:
  python3 open_loop_hot.py --url http://localhost:8080 --threads 16 --duration 120 --keys 10
"""

import argparse
import random
import threading
import time
from http.client import HTTPConnection, HTTPSConnection
from urllib.parse import urlparse


def worker(stop_at, parsed, paths, counters):
    use_https = parsed.scheme == "https"
    host = parsed.hostname
    port = parsed.port or (443 if use_https else 80)
    conn_cls = HTTPSConnection if use_https else HTTPConnection

    while time.time() < stop_at:
        path = random.choice(paths)
        try:
            conn = conn_cls(host, port, timeout=5)
            conn.request("GET", path)
            resp = conn.getresponse()
            # Consume and close quickly; ignore body
            resp.read()
            resp.close()
            conn.close()
            counters["ok"] += 1
        except Exception:
            counters["err"] += 1
            try:
                conn.close()
            except Exception:
                pass


def main():
    parser = argparse.ArgumentParser(description="Open-loop hot-cache load generator")
    parser.add_argument("--url", required=True, help="Base URL, e.g. http://localhost:8080")
    parser.add_argument("--threads", type=int, default=16, help="Number of load threads")
    parser.add_argument("--duration", type=int, default=120, help="Duration in seconds")
    parser.add_argument("--keys", type=int, default=10, help="Number of hot keys (/hot1..n)")
    args = parser.parse_args()

    parsed = urlparse(args.url)
    if not parsed.scheme or not parsed.hostname:
        raise SystemExit("Invalid URL; include scheme and host, e.g., http://localhost:8080")

    paths = [f"/hot{i}" for i in range(1, args.keys + 1)]
    stop_at = time.time() + args.duration
    counters = {"ok": 0, "err": 0}

    threads = []
    for _ in range(args.threads):
        t = threading.Thread(target=worker, args=(stop_at, parsed, paths, counters), daemon=True)
        t.start()
        threads.append(t)

    for t in threads:
        t.join()

    total = counters["ok"] + counters["err"]
    print("\n=== Open-Loop Hot Load ===")
    print(f"Threads: {args.threads}")
    print(f"Duration: {args.duration}s")
    print(f"Total Requests: {total}")
    print(f"OK: {counters['ok']}, Errors: {counters['err']}")
    if args.duration > 0:
        print(f"Throughput (approx): {total / args.duration:.2f} req/s")


if __name__ == "__main__":
    main()
