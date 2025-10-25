# CS744: HTTP-Based Caching Proxy Server (Implementation in C)

## Overview
This project implements a multithreaded HTTP proxy server in C with an in-memory Least Recently Used (LRU) cache.  
The proxy forwards client `GET` requests to an origin server running on `localhost:8000` and caches the responses in memory to improve performance on repeated access.

The system consists of:
- *Proxy Server (port 8080):* Handles client requests and caching.
- *Origin Server (port 8000):* Serves static files to the proxy.
- *Clients:* Send HTTP requests through the proxy (tested using `curl` and web browsers).

---

## How to Run:

### 1. Start the Origin Server
In a new terminal:
```bash
cd ~/PROXY_PROJECT/proxy_origin
python3 -m http.server 8000
```
This starts a simple HTTP server that serves files from the `proxy_origin` directory on port `8000`.

### 2. Building and Running the Proxy Server
In another terminal:
```bash
cd ~/proxy_project
make
./proxy
```
The proxy server will start listening on port `8080`.

## Sending Client Requests

You can use either `curl` or a web browser as a client.

### Using curl:
```bash
curl http://localhost:8080/index.html
curl http://localhost:8080/file1.html
```

### Using a browser:
Open the following URL in a web browser:
```
http://localhost:8080/index.html
```

## Expected Behavior
1. When a file is requested for the first time, the proxy retrieves it from the origin server.  
   The proxy log shows:
   ```
   Cache MISS for /index.html
   ```
2. When the same file is requested again, it is served directly from the proxy’s in-memory cache.  
   The proxy log shows:
   ```
   Cache HIT for /index.html
   ```

The system supports concurrent clients and maintains cache consistency using an LRU eviction policy.


## Example Execution Sequence
Terminal 1 (for Origin):
```bash
cd ~/proxy_origin
python3 -m http.server 8000
```

Terminal 2 (for Proxy):
```bash
cd ~/proxy_project
make
./proxy
```

Terminal 3 (for Client):
```bash
curl http://localhost:8080/index.html   # for seeing Cache MISS
curl http://localhost:8080/index.html   # for seeing Cache HIT
```

---

## Features
- Multithreaded proxy implementation using POSIX threads
- Thread-safe in-memory cache with LRU eviction
- Proper handling of partial writes and `EINTR`
- Graceful response to unsupported HTTP methods (`501 Not Implemented`)
- Easy testing using curl or browser clients
- Configurable constants:
  - `PROXY_PORT = 8080`
  - `ORIGIN_PORT = 8000`
  - `CACHE_CAPACITY = 5`

## Example Proxy Output
```
Proxy listening on port 8080
[Thread 140735620088640] Method=GET Path=/index.html
[Thread 140735620088640] Cache MISS for /index.html
[Thread 140735620088640] Cached /index.html (len=267)
[Thread 140735620088648] Method=GET Path=/index.html
[Thread 140735620088648] Cache HIT for /index.html
```

## Repository Structure
```
proxy_project/
│
├── proxy.c          # Proxy server implementation
├── Makefile         # Build configuration
├── README.md        # Project documentation
└── origin_server/   # Directory containing files served by the origin
```
