# ================================
#  Makefile for DECS KV Server
#  Final Phase-2 Version
# ================================

CC = gcc
CFLAGS = -std=gnu11 -Wall -O2 -pthread
# Try common Homebrew/Intel/M1 libpq locations
LDFLAGS = -L/opt/homebrew/opt/libpq/lib -L/opt/homebrew/lib -L/usr/local/opt/libpq/lib -L/usr/local/lib
INCLUDES = -I/opt/homebrew/opt/libpq/include -I/opt/homebrew/include -I/usr/local/opt/libpq/include -I/usr/local/include
LIBS = -lpq

TARGET = proxy_server
SRC = proxy_server.c
LOADGEN = loadgen
LOADGEN_SRC = loadgen.c
URL ?= http://localhost:8080
THREADS ?= 4
DURATION ?= 30s
HOT_KEYS ?= 10
VALUE_BYTES ?= 512

# ================================
#  Build Rules
# ================================
all: $(TARGET)
	@echo "[OK] Build complete: $(TARGET)"

$(TARGET): $(SRC)
	@echo "[BUILD] Compiling server..."
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(INCLUDES) $(LDFLAGS) $(LIBS)

$(LOADGEN): $(LOADGEN_SRC)
	@echo "[BUILD] Compiling load generator..."
	$(CC) $(CFLAGS) $(LOADGEN_SRC) -o $(LOADGEN)

run: $(TARGET)
	@echo "[RUN] Starting server on port 8080..."
	./$(TARGET)

run-getcpu: $(TARGET)
	@echo "[RUN] Starting server with GET_CPU_BURN_ITERS=200000 (CPU-bound GET path)..."
	@GET_CPU_BURN_ITERS=200000 PUTALL_IN_MEMORY=1 ./$(TARGET)

run-put: $(TARGET)
	@echo "[RUN] Starting server with PUTALL_IN_MEMORY=1 (PUTs skip DB writes)..."
	@PUTALL_IN_MEMORY=1 ./$(TARGET)

clean:
	@echo "[CLEAN] Removing binary..."
	@rm -f $(TARGET)
	@rm -f $(LOADGEN)

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

# ================================
#  Load Generators
# ================================
getpopular: $(LOADGEN)
	@echo "[LOAD] getpopular threads=$(THREADS) duration=$(DURATION) hot_keys=$(HOT_KEYS)"
	@./$(LOADGEN) getpopular --url $(URL) --threads $(THREADS) --duration $(DURATION) --hot-keys $(HOT_KEYS)

putall: $(LOADGEN)
	@echo "[LOAD] putall threads=$(THREADS) duration=$(DURATION) value_bytes=$(VALUE_BYTES)"
	@./$(LOADGEN) putall --url $(URL) --threads $(THREADS) --duration $(DURATION) --value-bytes $(VALUE_BYTES)

putall-sweep: $(LOADGEN)
	@echo "[SWEEP] putall threads list (default: 2 4 8 16 32 64 128) duration=$(DURATION)"
	@THREADS_LIST="$(THREADS_LIST)" DURATION="$(DURATION)" VALUE_BYTES="$(VALUE_BYTES)" URL="$(URL)" ./run_putall_sweep.sh

getpopular-sweep: $(LOADGEN)
	@echo "[SWEEP] getpopular threads list (default: 2 4 8 16 32 64 128) duration=$(DURATION) hot_keys=$(HOT_KEYS)"
	@THREADS_LIST="$(THREADS_LIST)" DURATION="$(DURATION)" HOT_KEYS="$(HOT_KEYS)" URL="$(URL)" ./run_getpopular_sweep.sh

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
#  Postgres Helpers
# ================================
pg-restart:
	@echo "[PG] Restarting postgresql (Homebrew service)..."
	@brew services restart postgresql || brew services restart postgresql@15 || brew services start postgresql@15

pg-status:
	@brew services list | grep postgres

psql:
	@echo "[PG] Opening psql shell to proxydb..."
	@psql -h localhost -p 5432 -U proxyuser -d proxydb

.PHONY: all clean run put get delete stats bulk getpopular putall checkport killport pg-restart pg-status psql
