# Changes Summary - New Workload Scripts and Database Management

## What Changed

### 1. Removed Old Workload Scripts
- `load_test_get_hot.lua` (removed)
- `load_test_get_cold.lua` (removed)
- `load_test_put.lua` (removed)
- `load_test_mixed.lua` (removed)

### 2. Created New Workload Scripts
- `load_test_cpu_bound.lua` - CPU-bound workload (hot cache GETs)
- `load_test_io_bound.lua` - IO-bound workload (cold cache GETs)
- `load_test_io_bound_put.lua` - IO-bound workload (PUT heavy)
- `load_test_mixed.lua` - Mixed workload (GET + PUT)

### 3. New Database Utility
- `db_utils.py` - Clears and populates database as needed for each workload

### 4. Updated Files
- `run_experiments.py` - Uses new scripts and db_utils.py
- `Makefile` - Updated commands to use new scripts

## Key Features

### Automatic Database Management
- **Clears database** before each workload
- **Populates only what's needed** for each workload type
- No leftover data from previous tests

### New Makefile Commands

```bash
# CPU-Bound Workload (automatically clears and populates DB)
make cpu-bound CONNECTIONS=100 DURATION=300s

# IO-Bound (Cold Cache) - automatically clears and populates 10,000 cold keys
make io-bound-cold CONNECTIONS=100 DURATION=300s

# IO-Bound (PUT Heavy) - clears DB, no pre-population needed
make io-bound-put CONNECTIONS=100 DURATION=300s

# Mixed Workload - clears and populates both hot and cold keys
make mixed CONNECTIONS=100 DURATION=300s

# Legacy aliases (still work)
make hot      # Same as cpu-bound
make cold     # Same as io-bound-cold
make putload  # Same as io-bound-put
```

### Database Utility Commands

```bash
# Clear database
python3 db_utils.py clear

# Populate for CPU-bound workload (10 hot keys)
python3 db_utils.py cpu_bound

# Populate for IO-bound cold workload (10,000 cold keys)
python3 db_utils.py io_bound_cold

# Prepare for IO-bound PUT workload (just clears)
python3 db_utils.py io_bound_put

# Populate for mixed workload (hot + cold keys)
python3 db_utils.py mixed
```

## Workload Details

### CPU-Bound Workload
- **Script:** `load_test_cpu_bound.lua`
- **Keys:** `/hot1`, `/hot2`, ..., `/hot10` (10 keys)
- **Data:** 10 hot keys pre-populated
- **Expected:** High cache hit ratio (>95%), CPU bottleneck

### IO-Bound (Cold Cache)
- **Script:** `load_test_io_bound.lua`
- **Keys:** `/cold_1`, `/cold_2`, ..., `/cold_10000` (10,000 keys)
- **Data:** 10,000 cold keys pre-populated
- **Expected:** High cache miss ratio (>80%), Disk I/O bottleneck

### IO-Bound (PUT Heavy)
- **Script:** `load_test_io_bound_put.lua`
- **Keys:** `/put_1`, `/put_2`, ... (created during test)
- **Data:** No pre-population needed
- **Expected:** Disk I/O bottleneck (DB writes)

### Mixed Workload
- **Script:** `load_test_mixed.lua`
- **Keys:** Mix of `/hot1-10` and `/cold_1-10000` for GETs, `/mixed_put_*` for PUTs
- **Data:** 10 hot keys + 1000 cold keys pre-populated
- **Expected:** Mixed CPU and IO pressure

## How It Works

1. **Before each workload test:**
   - Database is automatically cleared
   - Only required data is populated

2. **During test:**
   - Load generator runs with clean database state
   - No interference from previous test data

3. **After test:**
   - Results are collected
   - Database is ready for next workload

## Example Usage

```bash
# Terminal 1: Start server
make run

# Terminal 2: Run CPU-bound test
make cpu-bound CONNECTIONS=50 DURATION=30s

# Terminal 2: Run IO-bound test (automatically clears and repopulates)
make io-bound-cold CONNECTIONS=50 DURATION=30s

# Terminal 2: Run full experiments
make experiments
```

## Benefits

1. **Clean state** for each test
2. **No data pollution** between workloads
3. **Automatic setup** - no manual database management
4. **Consistent results** - same starting state every time
5. **Easy to use** - just run the make command

## Migration Notes

If you have old scripts or commands:
- `make hot` → `make cpu-bound` (or still works as alias)
- `make cold` → `make io-bound-cold` (or still works as alias)
- `make putload` → `make io-bound-put` (or still works as alias)

All old commands still work as aliases for backward compatibility.

