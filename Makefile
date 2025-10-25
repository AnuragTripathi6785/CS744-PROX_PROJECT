### Make File to automate the running and compilations of codes!!
CC = gcc
CFLAGS = -std=gnu11 -Wall -O2 -pthread

TARGET = proxy
SRC = proxy.c

.PHONY: all clean run origin

# Default build rule
all: $(TARGET)

# Compile the proxy
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

# Run the proxy server
run: $(TARGET)
	./$(TARGET)

# Start a simple origin server (for testing)
origin:
	cd proxy_origin && python3 -m http.server 8000

# Remove all the compiled binary files
clean:
	rm -f $(TARGET)
