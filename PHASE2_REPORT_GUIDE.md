# Phase 2 Demo & Report Guide (how I run it)

I keep this as my checklist to hit all rubric points: two different bottlenecks, 7 load levels, 5-minute runs, and graphs that show saturation (throughput flattens, latency rises, bottleneck maxes out).

## How I describe the architecture
- Two-tier HTTP KV server (`proxy_server`) with a thread pool and LRU cache backed by PostgreSQL.
- CPU-bound path: hot GETs all live in the cache (capacity 200), so worker CPU is the limiter.
- IO-bound path: cold GETs miss cache and trigger DB reads/writes, so disk is the limiter.
- I use a 10-connection DB pool and 8 worker threads; wrk + Lua drives load.

## The workloads I run
- CPU-bound: `load_test_cpu_bound.lua` against 10 hot keys. I expect >95% cache hit, CPU ~100%, low disk I/O.
- IO-bound (cold GET): `load_test_io_bound.lua` across `/cold_1..10000`. I expect <20% cache hit, high disk I/O, moderate CPU.
- Optional IO-bound (PUT-heavy): `load_test_io_bound_put.lua` writes 512B values to push DB writes.

## How I generate load
- wrk with Lua (closed-loop concurrency). Threads default to `min(connections, 4)`; I override with `THREADS=...` when needed.
- I reset DB state per workload using `db_utils.py`; CPU runs are auto cache-warmed inside `run_experiments.py`.
- I keep server and loadgen on different CPUs/machines; the server pins worker threads to core 0 for consistent CPU saturation.

## Load-test setup I stick to
- Load levels: 10, 25, 50, 100, 150, 200, 250 connections.
- Duration: 300s each (meets the 5-minute steady-state ask), with a 10s cooldown between points.
- Metrics: throughput, latency (mean/p50/p99), per-process CPU%, disk read/write MB/s. Saved to `experiment_results/results.csv` and `metrics.json`.
- Graphs land in `experiment_results/graphs/`.

## Commands I actually run
```bash
# Terminal 1 (server)
make clean && make
make run

# Terminal 2 (loadgen)
make experiments           # CPU-bound then IO-bound over all 7 load levels
make graphs                # PNGs in experiment_results/graphs/
```

## Quick repro for TAs (single load point)
```bash
# CPU-bound (hot cache)
make prefill-hot
make hot THREADS=4 CONNECTIONS=100 DURATION=300s

# IO-bound (cold cache)
make prefill-cold
make cold THREADS=4 CONNECTIONS=100 DURATION=300s
```
- For a 60s smoke test before demo: set `DURATION=60s`.
- Cache warmup for CPU-bound is automatic in `run_experiments.py`; `prefill-hot` + `make hot` also warms via Makefile.

## What I put in the report
- System architecture diagram (two-tier + cache + DB + thread pools; call out hot vs cold paths).
- Load generator design (wrk + Lua, closed-loop concurrency, CPU pinning, separate CPUs).
- Load-test setup: two terminals/CPUs, 7 load levels, 300s runs, 10s cooldown, metrics via psutil.
- Graphs from `experiment_results/graphs/`:
  - `throughput_vs_load.png`
  - `latency_vs_load.png`
  - `cpu_utilization_vs_load.png`
  - `disk_io_vs_load.png`
  - `cpu_bound_combined.png`, `io_bound_cold_combined.png`
- Table of numbers from `experiment_results/results.csv` (throughput, latency, CPU%, disk MB/s at each load).

## Slide outline I use (6 slides, ~5 minutes)
1) Architecture with hot vs cold path callouts.
2) Loadgen + methodology (wrk scripts, closed-loop, cache warmup, separate CPUs).
3) Load-test matrix (7 load levels, 300s runs, cooldown, metrics captured).
4) CPU-bound graphs: throughput flattening, latency rising, CPU≈100%, low disk.
5) IO-bound graphs: throughput flattening earlier, latency higher, disk≈100%, CPU moderate.
6) Repro commands + lessons/optimizations (optional).

## Demo-day checklist I follow
- Server running on one terminal/CPU; loadgen on another CPU/machine.
- `curl http://localhost:8080/__stats` shows cache hits climbing for CPU workload, DB reads climbing for IO workload.
- I keep expected numbers from `results.csv` handy so I can quote throughput at TA-requested load levels.
- Graphs ready in `experiment_results/graphs/`; I can rerun `make hot ...` or `make cold ...` live if asked.
