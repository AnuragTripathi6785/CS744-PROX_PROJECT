#!/bin/bash

SERVER="http://localhost:8080"
NUM_KEYS=${1:-100}   # if not specified, default = 100

echo "=== Bulk loading $NUM_KEYS key-value pairs to $SERVER ==="

for ((i=1; i<=NUM_KEYS; i++)); do
    VALUE="This is test value number $i"
    curl -s -X PUT "$SERVER/key_$i" -d "$VALUE" > /dev/null
    if (( i % 10 == 0 )); then
        echo "Loaded $i keys..."
    fi
done

echo "Done uploading $NUM_KEYS keys!"
echo "Now you can test GET requests and monitor cache hits/misses."
