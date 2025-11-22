# Quick Start Guide - CS744 Phase 2

## Quick Start (5 minutes)

### 1. Install Dependencies
```bash
# Install wrk
brew install wrk

# Install Python packages (choose one):
# Option 1: System-wide
make install-deps

# Option 2: Virtual environment (recommended)
./setup_venv.sh
source venv/bin/activate
```

### 2. Start Server
```bash
# Terminal 1
make clean && make && make run
```

### 3. Run Experiments (1-2 hours)
```bash
# Terminal 2
make experiments
```

### 4. Generate Graphs
```bash
# After experiments complete
make graphs
```

### 5. View Results
```bash
# Results are in:
ls experiment_results/
ls experiment_results/graphs/
```

## What You Get

After running experiments, you'll have:

1. **Results CSV:** `experiment_results/results.csv`
   - All metrics for each test
   - Throughput, latency, CPU, disk I/O

2. **Graphs:** `experiment_results/graphs/*.png`
   - Throughput vs Load
   - Latency vs Load  
   - CPU Utilization vs Load
   - Disk I/O vs Load
   - Combined graphs per workload

## Workloads

### CPU-Bound (Hot Cache GETs)
- Accesses 10 hot keys that fit in cache
- High cache hit ratio (>95%)
- CPU becomes bottleneck

### IO-Bound (Cold Cache GETs)
- Accesses random keys from large range
- High cache miss ratio (>80%)
- Disk I/O becomes bottleneck

## Load Levels

Tests run at 7 different load levels:
- 10, 25, 50, 100, 150, 200, 250 concurrent connections

Each test runs for **5 minutes** (as required).

## Quick Test (30 seconds)

To verify everything works:
```bash
# Terminal 1: Start server
make run

# Terminal 2: Quick test
make hot CONNECTIONS=50 DURATION=30s
```

## Full Instructions

See `PHASE2_INSTRUCTIONS.md` for detailed instructions.

## Checklist for Demo

- [ ] Run full experiment suite (`make experiments`)
- [ ] Generate graphs (`make graphs`)
- [ ] Review results in `experiment_results/`
- [ ] Prepare to reproduce specific load levels
- [ ] Know your expected throughput/latency numbers

## Troubleshooting

**Server won't start?**
```bash
make killport && make run
```

**Database errors?**
```bash
make db-status
make restart-db
```

**Experiments fail?**
- Check server is running: `curl http://localhost:8080/__stats`
- Check dependencies: `pip3 list | grep -E "psutil|matplotlib|numpy"`

## Files Created

- `load_test_get_hot.lua` - CPU-bound workload
- `load_test_get_cold.lua` - IO-bound workload  
- `load_test_put.lua` - IO-bound PUT workload
- `load_test_mixed.lua` - Mixed workload
- `run_experiments.py` - Comprehensive experiment runner
- `generate_graphs.py` - Graph generation script
- `PHASE2_INSTRUCTIONS.md` - Detailed instructions

Good luck!

