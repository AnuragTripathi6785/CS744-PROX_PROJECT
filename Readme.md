# CS744: HTTP-based Caching Proxy Server (Implementation in C)

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

## To send requests as a client

1.Open multiple terminals
2.On each terminal you can send requests of the the name of files you want to retrieve in this format: http://localhost:8080/<FILE-NAME> 
3. You can check on the proxy side terminal that when the file was not present in the cache then it shows 'Cache MISS' else 'Cache HIT'
