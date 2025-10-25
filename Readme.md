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
cd ~/proxy_project/proxy_origin
python3 -m http.server 8000
```
This starts a simple HTTP server that serves files from the `proxy_origin` directory on port `8000`.

### 2. Building and Running the Proxy Server
In another terminal:
```bash
cd ~/proxy_project 
gcc -std=gnu11 -Wall -O2 -pthread proxy.c -o proxy && ./proxy
```
The proxy server will start listening on port `8080`.

## Alternate way:
either you can do the above stuff or you can simply run the makefile's make commands as follows:
1. open the proxy_project directory then run these-> 
```make clean
   make
   make run
``` 
2. Now open another terminal and run this for origin server->
```
   make origin
```


## Sending Client Requests
Open a new terminal for client.
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
cd ~/proxy_project
make origin
```

Terminal 2 (for Proxy):
```bash
cd ~/proxy_project
make
make run
```

Terminal 3 (for Client):
```bash
curl http://localhost:8080/index.html   # for seeing Cache MISS
curl http://localhost:8080/index.html   # for seeing Cache HIT 

or open this link on your browser in the incognito-mode as the browser ususally caches your requests locally so next time even on a hit it might so happen that it doesn't send the response back to the proxy!!
```

## Features
- Multithreaded proxy implementation using POSIX threads
- Thread-safe in-memory cache with LRU eviction
- Proper handling of partial writes and `EINTR`
- Graceful response to unsupported HTTP methods (`501 Not Implemented`)
- Testing it using curl or browser clients

## Example Proxy Output
```
Proxy listening on port 8080
{Thread 140735620088640} Method=GET Path=/index.html
{Thread 140735620088640} Cache MISS for /index.html
{Thread 140735620088640} Cached /index.html (len=267)
{Thread 140735620088648} Method=GET Path=/index.html
{Thread 140735620088648} Cache HIT for /index.html
```

## Repository Structure
```
proxy_project/
│
├── proxy.c          # Proxy server implementation
├── Makefile         # Build configuration
├── README.md        # Project documentation
└── proxy_origin/   # Directory containing files served by the origin
```
