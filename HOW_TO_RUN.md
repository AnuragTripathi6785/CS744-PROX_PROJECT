# How to Run Server and Load Tests

## Quick Start (5 minutes)

### Step 1: Start the Server

**Open Terminal 1:**
```bash
cd /Users/shadymeee/proxy_project

# Build the server (if not already built)
make clean && make

# Start the server
make run
```

You should see:
```
Listening on port 8080
Access stats at: http://localhost:8080/__stats
Ready to accept connections...
```

**Keep this terminal open!** The server must be running for load tests to work.

---

### Step 2: Run a Quick Load Test

**Open Terminal 2** (new terminal window):

#### Option A: Quick CPU-Bound Test (30 seconds)
```bash
cd /Users/shadymeee/proxy_project

# Pre-populate hot keys
make prefill-hot

# Run hot cache GET test
make hot CONNECTIONS=50 DURATION=30s
```

#### Option B: Quick IO-Bound Test (30 seconds)
```bash
cd /Users/shadymeee/proxy_project

# Pre-populate cold keys (takes a minute)
make prefill-cold

# Run cold cache GET test
make cold CONNECTIONS=50 DURATION=30s
```

#### Option C: Quick PUT Test (30 seconds)
```bash
cd /Users/shadymeee/proxy_project

# Run PUT-heavy test (no pre-population needed)
make putload CONNECTIONS=50 DURATION=30s
```

---

## Full Load Test Suite (1-2 hours)

### For Phase 2 Demo - Complete Experiments

**Terminal 1:** Keep server running (`make run`)

**Terminal 2:**
```bash
cd /Users/shadymeee/proxy_project

# Run comprehensive experiments (CPU-bound and IO-bound)
make experiments
```

This will:
- Run CPU-bound workload at 7 load levels (10, 25, 50, 100, 150, 200, 250 connections)
- Run IO-bound workload at 7 load levels
- Each test runs for 5 minutes
- Collects all metrics automatically
- Saves results to `experiment_results/results.csv`

**After experiments complete:**
```bash
# Generate graphs
make graphs
```

Graphs will be saved in `experiment_results/graphs/`

---

## Individual Load Test Commands

### CPU-Bound (Hot Cache GETs)
```bash
# Pre-populate hot keys first
make prefill-hot

# Run test
make hot CONNECTIONS=100 DURATION=300s
```

### IO-Bound (Cold Cache GETs)
```bash
# Pre-populate cold keys first
make prefill-cold

# Run test
make cold CONNECTIONS=100 DURATION=300s
```

### IO-Bound (PUT Heavy)
```bash
# No pre-population needed
make putload CONNECTIONS=100 DURATION=300s
```

### Mixed Workload
```bash
# Pre-populate both hot and cold keys
make prefill-hot
make prefill-cold

# Run mixed test
make mixed CONNECTIONS=100 DURATION=300s
```

---

## Understanding the Parameters

- `CONNECTIONS`: Number of concurrent connections (load level)
  - Examples: 10, 50, 100, 200, 250
- `DURATION`: How long to run the test
  - Quick test: `30s` or `60s`
  - Full test: `300s` (5 minutes, as required)
- `THREADS`: Number of wrk threads (usually same as connections or 4)

---

## Monitoring During Tests

**Terminal 3** (optional - for monitoring):

```bash
# Watch server statistics
watch -n 1 'curl -s http://localhost:8080/__stats'

# OR check server process
top -pid $(pgrep proxy_server)

# OR check port usage
make checkport
```

---

## Example: Complete Workflow

```bash
# Terminal 1: Start server
cd /Users/shadymeee/proxy_project
make clean && make && make run

# Terminal 2: Run quick test
cd /Users/shadymeee/proxy_project
make prefill-hot
make hot CONNECTIONS=50 DURATION=60s

# Terminal 2: Run full experiments (after quick test works)
make experiments

# Terminal 2: Generate graphs
make graphs
```

---

## Troubleshooting

### Server won't start?
```bash
# Check if port is in use
make checkport

# Kill process on port 8080
make killport

# Try again
make run
```

### Load test fails?
```bash
# Check server is running
curl http://localhost:8080/__stats

# Check wrk is installed
which wrk

# Install wrk if needed
brew install wrk
```

### Database errors?
```bash
# Check PostgreSQL is running
make db-status

# Restart if needed
make restart-db
```

---

## Quick Reference

| Command | Description |
|---------|-------------|
| `make run` | Start server |
| `make hot` | Run CPU-bound test |
| `make cold` | Run IO-bound test |
| `make putload` | Run PUT-heavy test |
| `make mixed` | Run mixed workload |
| `make experiments` | Run full experiment suite |
| `make graphs` | Generate graphs |
| `make stats` | Check server statistics |
| `make killport` | Kill server process |

---

## For Demo with TAs

1. **Start server** in Terminal 1: `make run`
2. **Pre-populate data** in Terminal 2:
   ```bash
   make prefill-hot    # For CPU-bound
   make prefill-cold   # For IO-bound
   ```
3. **Run specific test** when TA asks:
   ```bash
   make hot CONNECTIONS=100 DURATION=300s
   ```
4. **Show results** from `experiment_results/results.csv`

---

That's it! You're ready to run load tests!

