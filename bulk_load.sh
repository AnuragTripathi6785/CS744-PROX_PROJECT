#!/bin/bash

SERVER="http://localhost:8080"
NUM_KEYS=${1:-100}
PREFIX=${2:-key_}   # default prefix is "key_"

echo "=== Bulk loading $NUM_KEYS keys with prefix '$PREFIX' to $SERVER ==="

for ((i=1; i<=NUM_KEYS; i++)); do
    VALUE="This is test value number $i"
    curl -s -X PUT "$SERVER/${PREFIX}${i}" --data-binary "$VALUE" > /dev/null

    if (( i % 100 == 0 )); then
        echo "Loaded $i keys..."
    fi
done

echo "Done uploading $NUM_KEYS keys!"
