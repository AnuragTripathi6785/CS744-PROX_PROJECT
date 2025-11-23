# Linux Bundle

This folder gives you a quick way to run the project on a Linux box with Postgres user/password `postgres/postgres`.

## Prereqs
- Postgres running locally on port 5432.
- Create database `proxydb` and table `kv_store` (created automatically on first run).

## Setup Postgres
```bash
sudo -u postgres psql -c "CREATE DATABASE proxydb;"
sudo -u postgres psql proxydb -c "CREATE TABLE IF NOT EXISTS kv_store (key VARCHAR(1024) PRIMARY KEY, value BYTEA, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP);"
```

## Build and run server
```bash
cd "$(dirname "$0")/.."
export DB_CONNINFO="host=localhost port=5432 dbname=proxydb user=postgres password=postgres"
make clean && make
make run
```

## CPU-bound quick test
```bash
make prefill-hot
wrk -t8 -c200 -d60s -s load_test_cpu_bound.lua --latency http://localhost:8080
```

## IO-bound PUT-heavy quick test
```bash
make io-bound-put THREADS=8 CONNECTIONS=200 DURATION=60s
```

## Full experiment sweeps
- CPU only: `python3 run_cpu_experiment.py`
- IO PUT only: `python3 run_io_experiment.py`
- Both: `python3 run_experiments.py`

Results go to `experiment_results/`. Use `make graphs` to render PNGs.
