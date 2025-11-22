# CS744 Phase 2: Load Testing Instructions

This document provides step-by-step instructions for running the Phase 2 load testing experiments and generating the required graphs for your demo.

## Prerequisites

### 1. Install Required Tools

**On macOS:**
```bash
# Install wrk (HTTP benchmarking tool)
brew install wrk

# Install Python dependencies (Option 1: System-wide with --user)
make install-deps

# OR (Option 2: Virtual environment - recommended)
./setup_venv.sh
source venv/bin/activate
```

**On Linux:**
```bash
# Install wrk
sudo apt-get install wrk  # Ubuntu/Debian
# OR
sudo yum install wrk      # CentOS/RHEL

# Install Python dependencies
pip3 install --user psutil matplotlib numpy
```

### 2. Database Setup

Ensure PostgreSQL is running and configured:

```bash
# Check if PostgreSQL is running
make db-status

# If not running, start it:
brew services start postgresql@15  # macOS
# OR
sudo systemctl start postgresql    # Linux

# Verify database exists
psql -U proxyuser -d proxydb -c "SELECT 1;"
```

If the database doesn't exist, create it:
```bash
createdb proxydb
psql proxydb << EOF
CREATE USER proxyuser WITH PASSWORD 'proxypass';
GRANT ALL PRIVILEGES ON DATABASE proxydb TO proxyuser;
GRANT ALL ON SCHEMA public TO proxyuser;
\q
EOF
```

## Running Experiments

### Step 1: Build and Start the Server

**Terminal 1 - Start the Server:**
```bash
# Build the server
make clean
make

# Start the server (runs on port 8080)
make run
```

The server should display:
```
Listening on port 8080
Access stats at: http://localhost:8080/__stats
Ready to accept connections...
```

**Important:** Keep this terminal open and the server running throughout all experiments.

### Step 2: Run Comprehensive Experiments

**Terminal 2 - Run Experiments:**
```bash
# Navigate to project directory
cd /path/to/proxy_project

# Run all experiments (this takes 1-2 hours)
make experiments

# OR run directly:
python3 run_experiments.py
```

**What this does:**
- Runs **CPU-bound workload** (hot cache GETs) at 7 different load levels: 10, 25, 50, 100, 150, 200, 250 connections
- Runs **IO-bound workload** (cold cache GETs) at the same 7 load levels
- Each test runs for **5 minutes** (300 seconds) as required
- Collects metrics: throughput, latency, CPU utilization, disk I/O
- Saves results to `experiment_results/results.csv` and `experiment_results/metrics.json`

**Expected Duration:**
- Each test: 5 minutes
- Cooldown between tests: 10 seconds
- Total tests: 14 (7 load levels × 2 workloads)
- **Total time: ~2 hours**

### Step 3: Generate Graphs

After experiments complete, generate graphs:

```bash
# Generate all graphs
make graphs

# OR run directly:
python3 generate_graphs.py
```

**Generated Graphs:**
- `throughput_vs_load.png` - Throughput comparison across workloads
- `latency_vs_load.png` - Latency comparison across workloads
- `cpu_utilization_vs_load.png` - CPU utilization comparison
- `disk_io_vs_load.png` - Disk I/O rates comparison
- `cpu_hot_combined.png` - All metrics for CPU-bound workload
- `io_cold_combined.png` - All metrics for IO-bound workload

All graphs are saved in `experiment_results/graphs/`

## Quick Test (For Verification)

If you want to quickly verify everything works before running the full suite:

```bash
# Terminal 1: Start server
make run

# Terminal 2: Run a quick test (30 seconds, single load level)
make hot THREADS=4 CONNECTIONS=50 DURATION=30s
```

## Understanding the Workloads

### Workload 1: CPU-Bound (Hot Cache GETs)
- **Script:** `load_test_get_hot.lua`
- **Behavior:** Accesses 10 hot keys that fit in the cache (capacity 200)
- **Expected:** High cache hit ratio (>95%), CPU becomes bottleneck
- **Bottleneck:** CPU utilization reaches ~99%

### Workload 2: IO-Bound (Cold Cache GETs)
- **Script:** `load_test_get_cold.lua`
- **Behavior:** Accesses random keys from a large range (1-10000)
- **Expected:** High cache miss ratio (>80%), frequent database reads
- **Bottleneck:** Disk I/O utilization reaches ~99%

### Optional: Mixed Workload
- **Script:** `load_test_mixed.lua`
- **Behavior:** 70% GET requests (mix of hot/cold), 30% PUT requests
- **Expected:** Mixed CPU and IO pressure

## Running on Different CPUs (Important for Demo)

The professor requires that the load generator and server run on **different CPUs**. On macOS, you can use CPU affinity:

**Option 1: Using taskset (if available on Linux)**
```bash
# Terminal 1: Pin server to CPU 0
taskset -c 0 ./proxy_server

# Terminal 2: Pin load generator to CPU 1
taskset -c 1 python3 run_experiments.py
```

**Option 2: Manual CPU Assignment (macOS)**
On macOS, you can manually assign processes to different CPU cores using Activity Monitor or by running the server and load generator on different machines/terminals and monitoring CPU usage.

**Option 3: Different Machines**
For the most accurate results, run the server on one machine and the load generator on another machine on the same network.

## Monitoring During Experiments

While experiments are running, you can monitor the system:

**Terminal 3 - Monitor Server:**
```bash
# Monitor server process
top -pid $(pgrep proxy_server)

# OR check server stats endpoint
watch -n 1 'curl -s http://localhost:8080/__stats'

# Check port usage
make checkport
```

**Monitor System Resources:**
```bash
# CPU and memory
top

# Disk I/O (macOS)
iostat -w 1

# Disk I/O (Linux)
iostat -x 1
```

## Results and Graphs

After running experiments, you'll have:

1. **CSV Results:** `experiment_results/results.csv`
   - Contains all metrics for each test
   - Can be opened in Excel/Google Sheets for analysis

2. **JSON Results:** `experiment_results/metrics.json`
   - Machine-readable format
   - Contains detailed metrics

3. **Graphs:** `experiment_results/graphs/*.png`
   - High-resolution PNG files (300 DPI)
   - Ready for inclusion in your report

## What the Graphs Show

The graphs demonstrate:

1. **Throughput vs Load:** Shows throughput flattening as system reaches capacity
2. **Latency vs Load:** Shows latency increasing as load increases
3. **CPU Utilization:** Shows CPU becoming fully utilized for CPU-bound workload
4. **Disk I/O:** Shows disk becoming fully utilized for IO-bound workload

These graphs prove that:
- CPU-bound workload saturates CPU (CPU utilization → 100%)
- IO-bound workload saturates disk I/O (disk utilization → 100%)
- Throughput plateaus when bottleneck is reached
- Latency increases as system approaches capacity

## Troubleshooting

### Server won't start
```bash
# Check if port 8080 is in use
make checkport

# Kill process on port 8080
make killport

# Try again
make run
```

### Database connection errors
```bash
# Check PostgreSQL is running
make db-status

# Test connection
psql -U proxyuser -d proxydb -c "SELECT 1;"

# Restart PostgreSQL
make restart-db
```

### Experiments fail or timeout
- Ensure server is running and responding: `curl http://localhost:8080/__stats`
- Check system resources: `top`, `iostat`
- Reduce load levels in `run_experiments.py` if system is overloaded

### Graphs not generating
- Ensure matplotlib is installed: `pip3 install matplotlib`
- Check that `experiment_results/results.csv` exists
- Verify Python version: `python3 --version` (should be 3.6+)

### Missing dependencies
```bash
# Install all dependencies
make install-deps

# OR manually:
pip3 install psutil matplotlib numpy
```

## Demo Preparation

Before the demo with TAs:

1. **Run full experiment suite** (1-2 hours)
2. **Generate all graphs**
3. **Review results** in `experiment_results/`
4. **Prepare to reproduce** specific load levels:
   ```bash
   # Example: Reproduce 100 connections for CPU-bound workload
   make prefill-hot
   make hot THREADS=4 CONNECTIONS=100 DURATION=300s
   ```

5. **Have server ready** to start quickly:
   ```bash
   make run  # In one terminal
   ```

6. **Know your numbers:** Review `results.csv` to know expected throughput/latency values

## Report Requirements Checklist

- [x] Two different workloads (CPU-bound and IO-bound)
- [x] Load tests at 5+ different load levels (we use 7)
- [x] Each test runs for 5 minutes (steady state)
- [x] Graphs showing throughput vs load level
- [x] Graphs showing latency vs load level
- [x] Graphs showing utilization (CPU, disk I/O) vs load level
- [x] Throughput flattening as capacity is reached
- [x] Latency increasing as load increases
- [x] Bottleneck being fully utilized

## Quick Reference Commands

```bash
# Build and run
make clean && make && make run

# Run experiments
make experiments

# Generate graphs
make graphs

# Run everything
make full-test

# Quick single test
make hot CONNECTIONS=100 DURATION=60s

# Check server status
make stats
make checkport
```

## Contact

If you encounter issues, check:
1. Server is running and responding
2. PostgreSQL is running
3. All dependencies are installed
4. System has enough resources (CPU, memory, disk)

Good luck with your demo!
