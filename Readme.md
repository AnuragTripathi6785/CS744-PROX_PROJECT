# CS744: HTTP-based Caching Proxy Server (C Implementation)

## Overview
This project implements a multithreaded HTTP proxy server in C with in-memory LRU caching.
It forwards GET requests to an origin server (localhost:8000) and caches responses for faster future access.

## How to Run
```bash
# Run origin server
cd ~/proxy_origin
python3 -m http.server 8000

# Build and run proxy
make
./proxy
