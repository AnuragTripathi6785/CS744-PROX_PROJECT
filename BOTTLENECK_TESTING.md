# Bottleneck Testing Guide

This guide explains how to run load tests to identify system bottlenecks (CPU-bound vs IO-bound).

## Quick Bottleneck Test (5 minutes)

### Step 1: Start the Server

**Terminal 1:**
```bash
cd /Users/shadymeee/proxy_project
make clean && make
make run
```

Keep this terminal open. The server must be running.

### Step 2: Test CPU-Bound Bottleneck

**Terminal 2:**
```bash
cd /Users/shadymeee/proxy_project

# Run CPU-bound test (automatically clears DB and loads 10 hot keys)
make cpu-bound CONNECTIONS=100 DURATION=60s
```

**What to look for:**
- High throughput (50,000+ req/s)
- Low latency (< 1ms average)
- High cache hit ratio (>95%)
- **CPU utilization should be high (80-100%)**
- Disk I/O should be low

### Step 3: Test IO-Bound Bottleneck

**Terminal 2:**
```bash
# Run IO-bound test (automatically clears DB and loads 10,000 cold keys)
make io-bound-cold CONNECTIONS=100 DURATION=60s
```

**What to look for:**
- Lower throughput (2,000-5,000 req/s)
- Higher latency (10-50ms average)
- High cache miss ratio (>80%)
- CPU utilization should be moderate (20-40%)
- **Disk I/O should be high (80-100%)**

## Monitoring Bottlenecks During Tests

### Option 1: Monitor Server Statistics

**Terminal 3:**
```bash
# Watch cache hit/miss ratio
watch -n 1 'curl -s http://localhost:8080/__stats'
```

Look for:
- Cache hit ratio: High (>95%) = CPU-bound, Low (<20%) = IO-bound
- DB reads: High = IO-bound (cold cache)
- DB writes: High = IO-bound (PUT workload)

### Option 2: Monitor System Resources

**Terminal 3:**
```bash
# Monitor CPU usage
top -pid $(pgrep proxy_server)

# OR use Activity Monitor (macOS GUI)
# Look for proxy_server process CPU usage
```

**What to observe:**
- CPU-bound: CPU usage 80-100%
- IO-bound: CPU usage 20-50%, but disk I/O is high

### Option 3: Monitor Disk I/O

**Terminal 3:**
```bash
# macOS - Monitor disk I/O
iostat -w 1

# OR use Activity Monitor (macOS GUI)
# Look at Disk tab for read/write activity
```

**What to observe:**
- CPU-bound: Low disk activity
- IO-bound: High disk read/write activity

## Complete Bottleneck Analysis

### Run Full Experiments

**Terminal 1:** Keep server running (`make run`)

**Terminal 2:**
```bash
cd /Users/shadymeee/proxy_project

# Run complete experiment suite
make experiments
```

This will:
1. Test CPU-bound workload at 7 load levels
2. Test IO-bound workload at 7 load levels
3. Collect metrics: throughput, latency, CPU, disk I/O
4. Save results to `experiment_results/results.csv`

### Generate Bottleneck Graphs

**Terminal 2:**
```bash
# Generate graphs showing bottlenecks
make graphs
```

Graphs will show:
- `cpu_utilization_vs_load.png` - CPU bottleneck visualization
- `disk_io_vs_load.png` - Disk I/O bottleneck visualization
- `throughput_vs_load.png` - Throughput saturation points
- `latency_vs_load.png` - Latency degradation

## Understanding the Results

### CPU-Bound Bottleneck Indicators

1. **Throughput:** Very high (50,000+ req/s)
2. **Latency:** Very low (< 1ms)
3. **Cache Hit Ratio:** >95%
4. **CPU Utilization:** 80-100% (bottleneck)
5. **Disk I/O:** Low (<10% utilization)

**Graph Pattern:**
- Throughput increases with load, then plateaus when CPU saturates
- Latency stays low until CPU saturates, then increases
- CPU utilization reaches 100% at saturation point

### IO-Bound Bottleneck Indicators

1. **Throughput:** Lower (2,000-5,000 req/s)
2. **Latency:** Higher (10-50ms)
3. **Cache Miss Ratio:** >80%
4. **CPU Utilization:** Moderate (20-50%)
5. **Disk I/O:** High (80-100% - bottleneck)

**Graph Pattern:**
- Throughput plateaus at lower level due to disk I/O
- Latency increases more quickly with load
- Disk I/O reaches 100% at saturation point
- CPU never fully utilized

## Quick Comparison Test

Run both tests back-to-back to see the difference:

```bash
# Terminal 1: Start server
make run

# Terminal 2: CPU-bound test
make cpu-bound CONNECTIONS=100 DURATION=60s

# Terminal 2: IO-bound test (automatically clears and repopulates)
make io-bound-cold CONNECTIONS=100 DURATION=60s
```

Compare the results:
- CPU-bound: Much higher throughput, lower latency
- IO-bound: Lower throughput, higher latency

## Bottleneck Test Checklist

- [ ] Server is running and responding
- [ ] CPU-bound test shows high CPU utilization
- [ ] IO-bound test shows high disk I/O
- [ ] Throughput differs significantly between workloads
- [ ] Cache hit ratio differs (high for CPU-bound, low for IO-bound)
- [ ] Graphs show clear bottleneck patterns

## Troubleshooting

### CPU not showing as bottleneck?
- Check if hot keys are in cache: `curl http://localhost:8080/__stats`
- Verify cache hit ratio is >95%
- Increase load level: `make cpu-bound CONNECTIONS=200 DURATION=60s`

### Disk I/O not showing as bottleneck?
- Check if cold keys are being accessed: Look at cache miss ratio
- Verify database is being queried: Check DB reads in stats
- Increase load level: `make io-bound-cold CONNECTIONS=200 DURATION=60s`

### Server not responding?
```bash
# Check if server is running
make checkport

# Restart if needed
make killport
make run
```

## Expected Results Summary

| Workload | Throughput | Latency | Cache Hit | Bottleneck |
|----------|-----------|---------|-----------|------------|
| CPU-Bound | 50,000+ req/s | <1ms | >95% | CPU (80-100%) |
| IO-Bound (Cold) | 2,000-5,000 req/s | 10-50ms | <20% | Disk I/O (80-100%) |
| IO-Bound (PUT) | 2,000-3,000 req/s | 20-100ms | N/A | Disk I/O (80-100%) |

## Next Steps

After identifying bottlenecks:
1. Review graphs in `experiment_results/graphs/`
2. Analyze `experiment_results/results.csv` for detailed metrics
3. Prepare report showing bottleneck identification
4. Be ready to reproduce results for demo

