# CS744: HTTP-Based Key-Value Server

Multithreaded HTTP key-value server written in C. Uses an in-memory LRU cache in front of a PostgreSQL backend and exposes GET, PUT, DELETE, and stats endpoints on port 8080.

## Features
- LRU cache with write-through semantics and RWLock synchronization
- PostgreSQL-backed persistence with connection pooling
- Worker-thread pool handling concurrent clients
- Stats endpoint at `/__stats` (cache hits/misses, ops counts)
- Bulk loader script for pre-seeding data

## Architecture
```
Clients (curl/custom loadgen/any HTTP)
        |
        v
HTTP server (thread pool) -- LRU cache (memory)
        |
        v
PostgreSQL database (kv_store table)
```

## Prerequisites
- PostgreSQL + libpq headers/libraries
- `gcc`, `make`, and standard POSIX build tools

Example setup (macOS):
```bash
brew install postgresql@15 libpq
brew services start postgresql@15
```

Example setup (Ubuntu/Debian):
```bash
sudo apt update
sudo apt install postgresql postgresql-contrib libpq-dev
sudo systemctl start postgresql
```

Database bootstrap:
```bash
createdb proxydb
psql proxydb <<'EOF'
CREATE USER proxyuser WITH PASSWORD 'proxypass';
GRANT ALL PRIVILEGES ON DATABASE proxydb TO proxyuser;
GRANT ALL ON SCHEMA public TO proxyuser;
EOF
```

## Build and Run
```bash
make clean
make
make run
```

## Basic Usage
- Store/update: `curl -X PUT http://localhost:8080/user1 -d "Alice"`
- Retrieve: `curl http://localhost:8080/user1`
- Delete: `curl -X DELETE http://localhost:8080/user1`
- Stats: `curl http://localhost:8080/__stats`
- Bulk load sample keys: `make bulk COUNT=100`

## Load Generators
- Hot GETs (CPU-bound path): `make getpopular THREADS=8 DURATION=60 HOT_KEYS=10`
  - Preloads the hot keys, then all threads randomly GET from that small set.
- PUT-heavy (IO-bound path): `make putall THREADS=8 DURATION=60 VALUE_BYTES=512`
  - Threads continuously PUT new keys with the chosen payload size.

`loadgen` (C, built via `make getpopular`/`make putall`) implements both workloads without external dependencies; tune threads to 2/4/8/16/32/64 to sweep throughput/latency/cpu.

## Repository Layout
- `proxy_server.c` – main server implementation (cache, HTTP parsing, DB pool)
- `Makefile` – build/run helpers and curl shortcuts
- `bulk_load.sh` – preloads the DB with sample key/value pairs
- `loadgen` – simple load generator (getpopular and putall workloads, built from loadgen.c)
- `.vscode/c_cpp_properties.json` – include path for libpq (optional editor aid)

## Notes
- All load-testing and benchmarking assets have been removed to keep the codebase focused on the core server. Only operational helpers remain.
