# CS744: HTTP-Based Key-Value Server with LRU Cache and PostgreSQL Backend

## Overview

This project implements a high-performance, multithreaded HTTP-based key-value server in C for the CS744 DECS (Design and Engineering of Computer Systems) course. The system supports GET, PUT, and DELETE operations and combines an in-memory LRU cache with a PostgreSQL backend for persistent storage.

The implementation demonstrates a complete two-tier architecture where the HTTP server acts as the frontend with caching capabilities, and PostgreSQL provides the persistent storage layer. The project was developed in two phases, with Phase 1 focusing on core functionality and Phase 2 adding performance evaluation capabilities.

---

## System Architecture

```
┌──────────────────────────────────────┐
│          Clients                     │
│  (curl / wrk / Lua scripts)          │
└──────────────┬───────────────────────┘
               │ HTTP Requests
               │ (GET/PUT/DELETE)
               ▼
┌──────────────────────────────────────┐
│    HTTP Key-Value Server (Port 8080) │
│  ┌────────────────────────────────┐  │
│  │   Thread Pool (Worker Threads) │  │
│  └────────────────────────────────┘  │
│  ┌────────────────────────────────┐  │
│  │   LRU Cache (In-Memory)        │  │
│  │   Capacity: 15 entries         │  │
│  └────────────────────────────────┘  │
│  ┌────────────────────────────────┐  │
│  │   DB Connection Pool (10 conn) │  │
│  └────────────────────────────────┘  │
└──────────────┬───────────────────────┘
               │ SQL Queries
               ▼
┌──────────────────────────────────────┐
│      PostgreSQL Database             │
│   (Persistent Storage - kv_store)    │
└──────────────────────────────────────┘
```

### Two-Tier Design
- **Tier 1:** HTTP server with in-memory LRU cache (fast access path)
- **Tier 2:** PostgreSQL database (persistent storage layer)

---

## Phase 1: Core Implementation

### Objective
The first phase focused on building a functional multithreaded HTTP key-value server with the following components:
- In-memory LRU cache for fast data access
- PostgreSQL backend for persistent storage
- Thread pool for handling concurrent client connections
- Database connection pooling for efficient database access
- Support for GET and PUT operations

### Features Implemented

**HTTP Operations:**
- GET: Retrieve key-value pairs
- PUT: Store or update key-value pairs

**Caching:**
- LRU (Least Recently Used) eviction policy
- Thread-safe implementation using RWLocks
- Cache capacity of 15 entries
- Write-through caching strategy

**Concurrency:**
- Thread pool to handle multiple concurrent clients
- Each client request is processed by a worker thread
- Connection pool with 10 pre-established PostgreSQL connections

**Data Persistence:**
- All key-value pairs stored in PostgreSQL database
- Automatic table creation on server startup
- Binary data support using PostgreSQL BYTEA type

### How It Works

**PUT Operation:**
1. Server receives PUT request with key and value
2. Data is written to PostgreSQL database first
3. Cache is updated with the new key-value pair
4. If cache is full, LRU eviction removes oldest entry
5. Success response sent to client

**GET Operation:**
1. Server receives GET request for a key
2. Cache is checked first (fast path)
3. If found in cache (cache hit), value returned immediately
4. If not in cache (cache miss), database is queried
5. Retrieved value is cached for future requests
6. Value returned to client

**Thread Safety:**
- RWLocks allow multiple concurrent readers
- Exclusive lock for write operations
- Prevents race conditions in cache updates

### Database Schema

The following table is automatically created when the server starts:

```sql
CREATE TABLE IF NOT EXISTS kv_store (
    key VARCHAR(1024) PRIMARY KEY,
    value BYTEA,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### Prerequisites

**On macOS:**
```bash
brew install postgresql@15 libpq
brew services start postgresql@15
```

**On Ubuntu/Linux:**
```bash
sudo apt update
sudo apt install postgresql postgresql-contrib libpq-dev
sudo systemctl start postgresql
```

### Database Setup

```bash
# Create database
createdb proxydb

# Create user and grant permissions
psql proxydb << EOF
CREATE USER proxyuser WITH PASSWORD 'proxypass';
GRANT ALL PRIVILEGES ON DATABASE proxydb TO proxyuser;
GRANT ALL ON SCHEMA public TO proxyuser;
\q
EOF
```

### Building and Running

```bash
# Clean any previous builds
make clean

# Compile the server
make

# Start the server
make run
```

Expected output when server starts:
```
=== HTTP-based Key-Value Server ===

Initializing database connection pool...
Database initialized successfully
   Connection pool size: 10
Server started successfully
   Listening on port: 8080
   Cache capacity: 15 entries
   Database: PostgreSQL (connected)

Access statistics at: http://localhost:8080/__stats

Ready to accept connections...
```

### Basic Usage (Phase 1)

**Store a key-value pair:**
```bash
curl -X PUT http://localhost:8080/user1 -d "Alice"
curl -X PUT http://localhost:8080/user2 -d "Bob"
```

**Retrieve a value:**
```bash
# First GET - cache miss, reads from database
curl http://localhost:8080/user1

# Second GET - cache hit, reads from cache
curl http://localhost:8080/user1
```

**View statistics:**
```bash
curl http://localhost:8080/__stats
```

Example statistics output:
```
=== KV Server Performance Statistics ===
Cache Hits: 5
Cache Misses: 2
Cache Hit Ratio: 71.43%
Current Cache Size: 2 / 15
Database Reads: 2
Database Writes: 2
Total GET requests: 7
Total PUT requests: 2
Total DELETE requests: 0
```

### Phase 1 outcomes:=

- Working HTTP key-value server with GET and PUT support
- In-memory LRU cache with thread-safe operations
- PostgreSQL integration for persistent storage
- Connection pooling for database efficiency
- Statistics endpoint for monitoring



## Phase 2: Performance Evaluation and Enhancement

### Objective
Phase 2 focused on enhancing the system with additional functionality and conducting comprehensive performance evaluation to identify system bottlenecks under different workloads.

### New Features Added

**DELETE Operation:**
- Remove key-value pairs from both cache and database
- Maintains consistency between cache and persistent storage

**Bulk Data Loading:**
- Script to pre-populate database with test data
- Supports loading arbitrary number of key-value pairs
- Useful for setting up load testing scenarios

**Load Testing Infrastructure:**
- Integration with wrk HTTP benchmarking tool
- Custom Lua scripts for different workload patterns
- Automated performance testing via Makefile

**Performance Monitoring:**
- Extended statistics tracking
- Resource utilization monitoring
- Bottleneck identification methodology

### DELETE Operation

**Using curl:**
```bash
curl -X DELETE http://localhost:8080/user1
```

**Using Makefile:**
```bash
make delete KEY=user1
```

The DELETE operation removes the key from both the PostgreSQL database and the in-memory cache to maintain consistency.

### Bulk Data Loading

To pre-populate the database for load testing:

```bash
# Load 100 key-value pairs
make bulk COUNT=100
```

This creates keys like `/key_1`, `/key_2`, etc., each with a 512-byte value.

### Load Testing Workloads

Three different workloads were designed to expose different performance bottlenecks:

#### Workload 1: PUT-Heavy (I/O Bound)

**Description:**
Generates continuous PUT requests with unique keys, forcing every request to write to the database.

**Expected Behavior:**
- High database write activity
- Disk I/O becomes bottleneck
- Lower throughput due to disk latency

**Run test:**
```bash
make putload
```

**Monitoring:**
```bash
# Monitor disk I/O
make checkport

# Monitor CPU
top -p $(pgrep kv_server)
```

#### Workload 2: GET Hot (CPU/Memory Bound)

**Description:**
Repeatedly accesses a small set of keys (10 keys) that all fit in the cache, resulting in very high cache hit ratio.

**Expected Behavior:**
- Cache hit ratio over 95%
- Minimal database access
- CPU becomes bottleneck
- Very high throughput

**Setup:**
```bash
# Pre-populate hot keys
for i in {1..10}; do
  curl -X PUT http://localhost:8080/hot$i -d "DATA$i"
done
```

**Run test:**
```bash
make hot
```

#### Workload 3: GET Cold (I/O Bound)

**Description:**
Requests random keys with high probability of cache misses, forcing frequent database reads.

**Expected Behavior:**
- High cache miss ratio
- Heavy database read activity
- Disk I/O becomes bottleneck
- Lower throughput

**Run test:**
```bash
make cold
```

### Performance Results

After running load tests at different connection levels (20, 50, 100, 150, 200 concurrent connections), the following bottlenecks were identified:

| Workload      | Throughput     | Cache Hit % | Bottleneck | Resource @ High Util |
|---------------|----------------|-------------|------------|----------------------|
| PUT Heavy     | ~2,500 req/s   | N/A         | I/O        | Disk (writes ~99%)   |
| GET Hot       | ~50,000 req/s  | >95%        | CPU        | CPU (~99%)           |
| GET Cold      | ~3,000 req/s   | <20%        | I/O        | Disk (reads ~99%)    |

### Key Observations

**Cache Effectiveness:**
The GET hot workload achieves approximately 20x higher throughput compared to GET cold, demonstrating the dramatic performance impact of caching. When data is served from memory, the system is only limited by CPU processing capacity rather than disk I/O.

**I/O Bottleneck:**
Both PUT-heavy and GET cold workloads saturate disk I/O, with disk utilization reaching 99%. This occurs because these workloads bypass or miss the cache, forcing every request to access the database. Throughput plateaus at around 2,500-3,000 requests per second.

**CPU Bottleneck:**
The GET hot workload shows CPU utilization at 99% while disk remains mostly idle. This proves that when cache hit ratio is high, the system shifts from being I/O bound to CPU bound, and throughput increases dramatically.

**Write-Through Trade-off:**
The write-through caching strategy ensures data consistency but results in slower writes since every PUT must wait for database confirmation. An alternative write-back strategy would improve write performance but at the cost of potential data loss.

### Makefile Commands

| Command                          | Description                              |
|----------------------------------|------------------------------------------|
| `make clean`                     | Remove compiled files                    |
| `make`                           | Compile the server                       |
| `make run`                       | Start the server                         |
| `make put KEY=k VALUE=v`         | Insert/update a key-value pair           |
| `make get KEY=k`                 | Retrieve a value by key                  |
| `make delete KEY=k`              | Delete a key-value pair                  |
| `make stats`                     | Display cache and database statistics    |
| `make bulk COUNT=n`              | Bulk load n key-value pairs              |
| `make putload`                   | Run PUT-heavy load test                  |
| `make hot`                       | Run hot GET load test                    |
| `make cold`                      | Run cold GET load test                   |

### Phase 2 Deliverables

- DELETE operation implementation
- Three distinct workload scripts (PUT heavy, GET hot, GET cold)
- Performance testing infrastructure with wrk integration
- Identification of two different bottlenecks (I/O bound vs CPU bound)
- Throughput and latency measurements at multiple load levels
- Resource utilization analysis
- Demo with TA showing complete system with performance results

---

## Repository Structure

```
proxy_project/
│
├── proxy_server.c              # Main server implementation
├── Makefile                    # Build automation and testing
├── README.md                   # This file
│
├── load_test_put.lua           # PUT-heavy workload script
├── load_test_get_hot.lua       # Hot GET workload script
├── load_test_get_cold.lua      # Cold GET workload script
│
└── bulk_load.sh                # Bulk data loading script
|___ .vscode
          |__ c_cpp_properties.json.   #to include the library /opt/homebrew/opt/libpq/include
```

---

## Implementation Details

### LRU Cache Design

The cache is implemented using a combination of:
- **Hash table** for O(1) key lookup
- **Doubly linked list** for O(1) LRU operations

Operations:
- **Lookup:** Hash the key and search the hash bucket
- **Update:** Move accessed node to front of list
- **Eviction:** Remove node from tail of list
- **Thread Safety:** RWLocks allow concurrent readers, exclusive writer

### Database Connection Pool

Instead of creating a new database connection for each request, the server maintains a pool of 10 pre-established connections. Connections are allocated in round-robin fashion, reducing the overhead of connection establishment and improving throughput.

### Thread Pool Architecture

The server uses a thread pool model where:
- Fixed number of worker threads are created at startup
- Incoming client connections are accepted by the main thread
- Each connection is dispatched to an available worker thread
- Worker threads process requests and return to the pool

This approach provides better resource control compared to spawning a new thread per connection.

---

## Design Decisions

### Why Write-Through Caching?

I chose write-through caching where PUT operations write to both cache and database synchronously. This ensures that the cache and database are always consistent, and no data is lost if the server crashes. The trade-off is slower write performance since we must wait for database confirmation.

An alternative would be write-back caching where writes go to cache first and are flushed to database later. This would improve write throughput but introduces complexity in handling failures and potential data loss.

### Why PostgreSQL?

I chose PostgreSQL because:
- It's a production-grade database with good performance
- The libpq C library has clear documentation
- It supports BYTEA type for storing arbitrary binary data
- It has UPSERT functionality (INSERT ... ON CONFLICT)

MySQL would have worked similarly well, but I found PostgreSQL's C API easier to work with.

### Why RWLocks Instead of Mutex?

Initially, I implemented cache synchronization using a simple mutex, which serialized all cache access. After profiling, I switched to RWLocks which allow multiple concurrent readers but exclusive writers. Since GET requests (reads) are typically more common than PUT requests (writes), this improved performance by about 40% for read-heavy workloads.

---

## Troubleshooting

### Connection to database failed

Check if PostgreSQL is running:
```bash
brew services list | grep postgres
```

Start if not running:
```bash
brew services start postgresql@15
```

Test connection manually:
```bash
psql -U proxyuser -d proxydb -h localhost
```

### Library not found for -lpq

On macOS, you may need to specify the library path explicitly:
```bash
gcc -std=gnu11 -Wall -O2 -pthread proxy_kv_server.c -o kv_server \
  -I$(brew --prefix libpq)/include \
  -L$(brew --prefix libpq)/lib \
  -lpq
```

### Port 8080 already in use

Kill the process using the port:
```bash
sudo fuser -k 8080/tcp
```

Or change the SERVER_PORT constant in the code.

### VS Code shows red squiggles on libpq-fe.h

Create `.vscode/c_cpp_properties.json`:
```json
{
    "configurations": [
        {
            "name": "Mac",
            "includePath": [
                "${workspaceFolder}/**",
                "/opt/homebrew/opt/libpq/include"
            ],
            "compilerPath": "/usr/bin/gcc",
            "cStandard": "gnu11"
        }
    ],
    "version": 4
}
```

Then reload VS Code window.

---

## Project Requirements Met

### Multi-Tier System
- Tier 1: HTTP server with in-memory cache
- Tier 2: PostgreSQL database for persistence

### Multiple Request Types
- GET: Read operation
- PUT: Create/Update operation  
- DELETE: Remove operation

### Different Performance Bottlenecks
1. **I/O Bound:** PUT heavy and GET cold workloads saturate disk
2. **CPU Bound:** GET hot workload with high cache hits saturates CPU

### Load Testing
- Multiple workloads tested (PUT heavy, GET hot, GET cold)
- Tests run at 5 different load levels
- Throughput and latency measured
- Resource utilization monitored

---

## Conclusion

This project successfully implements a two-tier HTTP-based key-value server with caching optimization. Phase 1 established the core functionality with GET/PUT operations, LRU cache, and database integration. Phase 2 added DELETE support and comprehensive performance evaluation, identifying distinct I/O bound and CPU bound bottlenecks depending on the workload characteristics.

The system demonstrates that caching can dramatically improve performance for read-heavy workloads, with throughput increasing by 20x when data is served from memory rather than disk. The different workloads clearly show how system bottlenecks shift between disk I/O and CPU depending on cache effectiveness.

---

## Author

CS744 DECS Autumn 2025 Project

---

## Acknowledgments

- PostgreSQL documentation and libpq library
- wrk HTTP benchmarking tool
- CS744 course staff and TAs