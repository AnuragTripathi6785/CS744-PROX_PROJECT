# Fixing CPU-Bound Test Issues

## Problems Identified

1. **Cache Hit Ratio Too Low (39.68% instead of >95%)**
   - Keys are being evicted from cache
   - Cache might have old data from previous tests

2. **Socket Read Errors (1,903,500 errors)**
   - Server might be overwhelmed
   - Too many concurrent connections

## Solutions

### Solution 1: Restart Server to Clear Cache

Before running CPU-bound test, restart the server to ensure clean cache:

```bash
# Terminal 1: Stop server (Ctrl+C), then restart
make killport
make run
```

### Solution 2: Reduce Load Level

Try with fewer connections to avoid socket errors:

```bash
# Try with 50 connections instead of 100
make cpu-bound CONNECTIONS=50 DURATION=60s

# Or even lower for testing
make cpu-bound CONNECTIONS=25 DURATION=60s
```

### Solution 3: Use Fewer Threads

Reduce wrk threads to match connections:

```bash
# Use 4 threads instead of 20
wrk -t4 -c50 -d60s -s load_test_cpu_bound.lua --latency http://localhost:8080
```

### Solution 4: Verify Cache is Working

After populating hot keys, check cache:

```bash
# Check stats before test
curl http://localhost:8080/__stats

# Make a few manual requests to warm cache
for i in {1..10}; do curl -s http://localhost:8080/hot$i > /dev/null; done

# Check stats again - should show cache hits
curl http://localhost:8080/__stats
```

## Recommended Test Procedure

1. **Restart server** (clears cache):
   ```bash
   make killport
   make run
   ```

2. **Populate hot keys and warm cache**:
   ```bash
   make prefill-cpu
   # Warm cache by accessing keys
   for i in {1..10}; do curl -s http://localhost:8080/hot$i > /dev/null; done
   ```

3. **Run test with moderate load**:
   ```bash
   make cpu-bound CONNECTIONS=50 DURATION=60s
   ```

4. **Check results**:
   - Cache hit ratio should be >95%
   - Socket errors should be minimal (<1%)
   - Throughput should be high (20,000+ req/s)

## Expected Correct Output

For CPU-bound test, you should see:
- **Cache Hit Ratio:** >95%
- **Throughput:** 20,000-50,000 req/s
- **Avg Latency:** <5ms
- **P50 Latency:** <2ms
- **Socket Errors:** <1% of requests
- **CPU Usage:** High (80-100%)

## If Cache Hit Ratio Still Low

The cache might be too small (currently 200 entries) or keys are being evicted. Try:

1. Access only 5 keys instead of 10 (to ensure they stay in cache)
2. Reduce load to avoid cache thrashing
3. Check if other keys are in cache from previous tests
