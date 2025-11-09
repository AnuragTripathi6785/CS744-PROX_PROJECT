
# Makefile for HTTP-based Key-Value Proxy Server:-  

# Compiler and flags
CC = gcc
CFLAGS = -std=gnu11 -Wall -O2 -pthread
LDFLAGS = -L/opt/homebrew/opt/libpq/lib
INCLUDES = -I/opt/homebrew/opt/libpq/include
LIBS = -lpq

# Executable name
TARGET = proxy_server

# Source file
SRC = proxy_server.c

# Default rule
all: $(TARGET)
	@echo "Build complete: $(TARGET)"

# Build rule
$(TARGET): $(SRC)
	@echo "Building $(TARGET)..."
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(INCLUDES) $(LDFLAGS) $(LIBS)

# Run the server
run: $(TARGET)
	@echo "Starting server on port 8080..."
	./$(TARGET)

# Clean compiled files
clean:
	@rm -f $(TARGET)
	@echo "Cleaned up compiled files."

# Restart PostgreSQL service (macOS Homebrew specific)
restart-db:
	@echo "Restarting PostgreSQL service..."
	brew services restart postgresql@14 || brew services start postgresql@14

# Show PostgreSQL service status
db-status:
	@brew services list | grep postgres

# Connect to database using psql
psql:
	psql -U proxyuser -d proxydb
# PUT a key-value pair
put:
	@echo "Sending PUT request: $(KEY) = $(VALUE)"
	@curl -s -X PUT http://localhost:8080/$(KEY) -d "$(VALUE)" && echo "\nPUT Success"

# GET a key
get:
	@echo "Fetching value for key: $(KEY)"
	@curl -s http://localhost:8080/$(KEY) && echo "\nGET Done"

# DELETE a key
delete:
	@echo "Deleting key: $(KEY)"
	@curl -s -X DELETE http://localhost:8080/$(KEY) && echo "\nDELETE Done"

# Check Server Stats
stats:
	@echo "Fetching server stats..."
	@curl -s http://localhost:8080/__stats && echo "\nStats Fetched"

# Bulk Load Script Runner
bulk:
	@echo "Running bulk load script with count = $(COUNT)"
	@./bulk_load.sh $(COUNT)

# check and kill port 
checkport:
	lsof -i :8080

killport:
	@echo "Killing process on port 8080..."
	@PID=$$(lsof -t -i:8080); if [ "$$PID" != "" ]; then kill -9 $$PID; fi

# loadtesting and generation:
putload:
	wrk -t4 -c50 -d30s -s load_test_put.lua http://localhost:8080

hot:
	wrk -t4 -c50 -d30s -s load_test_get_hot.lua http://localhost:8080

cold:
	wrk -t4 -c50 -d30s -s load_test_get_cold.lua http://localhost:8080

.PHONY: all clean run restart-db db-status psql
