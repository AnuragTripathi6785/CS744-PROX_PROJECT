# ================================
#  Makefile for DECS KV Server
#  Final Phase-2 Version
# ================================

CC = gcc
CFLAGS = -std=gnu11 -Wall -O2 -pthread
LDFLAGS = -L/opt/homebrew/opt/libpq/lib
INCLUDES = -I/opt/homebrew/opt/libpq/include
LIBS = -lpq

TARGET = proxy_server
SRC = proxy_server.c

# ================================
#  Build Rules
# ================================
all: $(TARGET)
	@echo "[OK] Build complete: $(TARGET)"

$(TARGET): $(SRC)
	@echo "[BUILD] Compiling server..."
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(INCLUDES) $(LDFLAGS) $(LIBS)

run: $(TARGET)
	@echo "[RUN] Starting server on port 8080..."
	./$(TARGET)

clean:
	@echo "[CLEAN] Removing binary..."
	@rm -f $(TARGET)

# ================================
#  Database Helpers
# ================================
restart-db:
	brew services restart postgresql@14 || brew services start postgresql@14

db-status:
	brew services list | grep postgres

psql:
	psql -U proxyuser -d proxydb

# ================================
#  Basic Operations
# ================================
put:
	@echo "[PUT] $(KEY) = $(VALUE)"
	@curl -s -X PUT http://localhost:8080/$(KEY) -d "$(VALUE)" && echo ""

get:
	@echo "[GET] $(KEY)"
	@curl -s http://localhost:8080/$(KEY) && echo ""

delete:
	@echo "[DELETE] $(KEY)"
	@curl -s -X DELETE http://localhost:8080/$(KEY) && echo ""

stats:
	@echo "[STATS]"
	@curl -s http://localhost:8080/__stats && echo ""

# ================================
#  Bulk Loading (for testing)
# ================================
bulk:
	@echo "[BULK] loading $(COUNT) entries..."
	@./bulk_load.sh $(COUNT) $(PREFIX)

# Database utilities (use db_utils.py)
clear-db:
	@echo "[DB] Clearing database..."
	@python3 db_utils.py clear

prefill-cpu:
	@echo "[DB] Populating for CPU-bound workload..."
	@python3 db_utils.py cpu_bound

prefill-io-cold:
	@echo "[DB] Populating for IO-bound (cold) workload..."
	@python3 db_utils.py io_bound_cold

prefill-io-put:
	@echo "[DB] Preparing for IO-bound (PUT) workload..."
	@python3 db_utils.py io_bound_put

prefill-mixed:
	@echo "[DB] Populating for mixed workload..."
	@python3 db_utils.py mixed

# Legacy aliases
prefill-hot: prefill-cpu
prefill-cold: prefill-io-cold

# ================================
#  Load Test Infrastructure
#  (DECS: 5 minutes recommended)
# ================================

THREADS ?= 20
CONNECTIONS ?= $(THREADS)
DURATION ?= 300      # 5 minutes, DECS requirement

# Helper: Add 's' suffix to duration if not already present
DURATION_WITH_UNIT = $(if $(findstring s,$(DURATION)),$(DURATION),$(DURATION)s)

# CPU-Bound Workload (Hot Cache GETs)
cpu-bound:
	@echo "[CPU-BOUND] Threads=$(THREADS) Conns=$(CONNECTIONS) Dur=$(DURATION_WITH_UNIT)"
	@python3 db_utils.py cpu_bound
	@echo "[CPU-BOUND] Warming cache..."
	@for i in 1 2 3 4 5 6 7 8 9 10; do \
		curl -s http://localhost:8080/hot$$i > /dev/null; \
	done
	@echo "[CPU-BOUND] Starting load test..."
	wrk -t$(THREADS) -c$(CONNECTIONS) -d$(DURATION_WITH_UNIT) \
	    -s load_test_cpu_bound.lua --latency http://localhost:8080

# IO-Bound Workload (Cold Cache GETs)
io-bound-cold:
	@echo "[IO-BOUND-COLD] Threads=$(THREADS) Conns=$(CONNECTIONS) Dur=$(DURATION_WITH_UNIT)"
	@python3 db_utils.py io_bound_cold
	wrk -t$(THREADS) -c$(CONNECTIONS) -d$(DURATION_WITH_UNIT) \
	    -s load_test_io_bound.lua --latency http://localhost:8080

# IO-Bound Workload (PUT Heavy)
io-bound-put:
	@echo "[IO-BOUND-PUT] Threads=$(THREADS) Conns=$(CONNECTIONS) Dur=$(DURATION_WITH_UNIT)"
	@python3 db_utils.py io_bound_put
	wrk -t$(THREADS) -c$(CONNECTIONS) -d$(DURATION_WITH_UNIT) \
	    -s load_test_io_bound_put.lua --latency http://localhost:8080

# Mixed Workload
mixed:
	@echo "[MIXED-LOAD] Running GET+PUT workload"
	@python3 db_utils.py mixed
	wrk -t$(THREADS) -c$(CONNECTIONS) -d$(DURATION_WITH_UNIT) \
	    -s load_test_mixed.lua --latency http://localhost:8080

# Legacy aliases for backward compatibility
hot: cpu-bound
cold: io-bound-cold
putload: io-bound-put

# Run all workloads at one config
test-all:
	$(MAKE) hot THREADS=$(THREADS) CONNECTIONS=$(CONNECTIONS)
	$(MAKE) cold THREADS=$(THREADS) CONNECTIONS=$(CONNECTIONS)
	$(MAKE) putload THREADS=$(THREADS) CONNECTIONS=$(CONNECTIONS)

# ================================
#  Port Management
# ================================
checkport:
	lsof -i :8080

killport:
	@PID=$$(lsof -t -i:8080); \
	if [ "$$PID" != "" ]; then \
		echo "[KILL] Killing PID $$PID"; \
		kill -9 $$PID; \
	fi

# ================================
#  Phase 2: Comprehensive Experiments
# ================================

# Run full experiment suite (CPU-bound and IO-bound workloads)
experiments:
	@echo "[EXPERIMENTS] Running comprehensive load tests..."
	@echo "[EXPERIMENTS] This will take approximately 1-2 hours..."
	@python3 run_experiments.py

# Generate graphs from experiment results
graphs:
	@echo "[GRAPHS] Generating graphs from experiment results..."
	@python3 generate_graphs.py

# Run experiments and generate graphs
full-test: experiments graphs
	@echo "[DONE] Experiments complete! Check experiment_results/ directory"

# Install Python dependencies for experiments
install-deps:
	@echo "[DEPS] Installing Python dependencies..."
	@pip3 install --break-system-packages --user psutil matplotlib numpy || \
	 (echo "[ERROR] Failed to install dependencies."; \
	  echo "[ERROR] Alternative: Create a virtual environment:"; \
	  echo "  python3 -m venv venv"; \
	  echo "  source venv/bin/activate"; \
	  echo "  pip install psutil matplotlib numpy")

.PHONY: all clean run put get delete stats test-all cpu-bound io-bound-cold io-bound-put mixed hot cold putload experiments graphs full-test install-deps clear-db prefill-cpu prefill-io-cold prefill-io-put prefill-mixed prefill-hot prefill-cold
