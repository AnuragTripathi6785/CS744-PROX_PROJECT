#!/usr/bin/env python3
"""
Database utility functions for clearing and populating the database
"""

import subprocess
import sys
import time

SERVER_URL = "http://localhost:8080"

def clear_database():
    """Clear all data from the database using DELETE requests"""
    print("[DB] Clearing database...")
    
    # Delete hot keys (format: /hot1, /hot2, etc.)
    for i in range(1, 11):
        subprocess.run(
            ["curl", "-s", "-X", "DELETE", f"{SERVER_URL}/hot{i}"],
            capture_output=True
        )
    
    # Delete cold keys (format: /cold_1, /cold_2, etc.)
    # Delete in batches to avoid taking too long
    for i in range(1, 10001, 100):  # Delete every 100th key
        subprocess.run(
            ["curl", "-s", "-X", "DELETE", f"{SERVER_URL}/cold_{i}"],
            capture_output=True
        )
    
    # Delete PUT keys (format: /put_1, /put_2, etc.)
    for i in range(1, 10001, 100):  # Delete in batches
        subprocess.run(
            ["curl", "-s", "-X", "DELETE", f"{SERVER_URL}/put_{i}"],
            capture_output=True
        )
    
    # Delete mixed keys
    for i in range(1, 10001, 100):
        subprocess.run(
            ["curl", "-s", "-X", "DELETE", f"{SERVER_URL}/mixed_put_{i}"],
            capture_output=True
        )
    
    print("[DB] Database cleared")
    time.sleep(1)  # Brief pause

def populate_cpu_bound():
    """Populate database for CPU-bound workload (hot cache GETs)"""
    print("[DB] Populating database for CPU-bound workload...")
    print("[DB] Loading 10 hot keys that fit in cache (8KB values for CPU work)...")
    
    big_payload = "X" * (8 * 1024)  # 8KB payload to increase CPU copy/serialization work
    for i in range(1, 11):
        value = big_payload
        subprocess.run(
            ["curl", "-s", "-X", "PUT", f"{SERVER_URL}/hot{i}", "-d", value],
            capture_output=True
        )
        if i % 5 == 0:
            print(f"[DB] Loaded {i}/10 hot keys...")
    
    print("[DB] CPU-bound workload data ready")
    time.sleep(1)

def populate_io_bound_cold():
    """Populate database for IO-bound workload (cold cache GETs)"""
    print("[DB] Populating database for IO-bound workload (cold cache)...")
    print("[DB] Loading 10,000 cold keys (this may take a minute)...")
    
    # Load keys in batches
    batch_size = 100
    total_keys = 10000
    
    for start in range(1, total_keys + 1, batch_size):
        end = min(start + batch_size - 1, total_keys)
        for i in range(start, end + 1):
            value = f"COLD_DATA_{i}" * 10
            subprocess.run(
                ["curl", "-s", "-X", "PUT", f"{SERVER_URL}/cold_{i}", "-d", value],
                capture_output=True
            )
        
        if start % 1000 == 1:
            print(f"[DB] Loaded {end}/{total_keys} cold keys...")
    
    print("[DB] IO-bound (cold) workload data ready")
    time.sleep(1)

def populate_io_bound_put():
    """No pre-population needed for PUT workload - it creates keys on the fly"""
    print("[DB] IO-bound (PUT) workload: No pre-population needed")
    print("[DB] Keys will be created during the test")
    time.sleep(1)

def populate_mixed():
    """Populate database for mixed workload"""
    print("[DB] Populating database for mixed workload...")
    
    # Load hot keys
    print("[DB] Loading 10 hot keys...")
    for i in range(1, 11):
        value = f"DATA{i}" * 50
        subprocess.run(
            ["curl", "-s", "-X", "PUT", f"{SERVER_URL}/hot{i}", "-d", value],
            capture_output=True
        )
    
    # Load cold keys (sample - enough for cache misses)
    print("[DB] Loading 1000 cold keys...")
    for i in range(1, 1001):
        value = f"COLD_DATA_{i}" * 10
        subprocess.run(
            ["curl", "-s", "-X", "PUT", f"{SERVER_URL}/cold_{i}", "-d", value],
            capture_output=True
        )
        if i % 200 == 0:
            print(f"[DB] Loaded {i}/1000 cold keys...")
    
    print("[DB] Mixed workload data ready")
    time.sleep(1)

def main():
    """CLI interface"""
    if len(sys.argv) < 2:
        print("Usage: python3 db_utils.py <command>")
        print("Commands: clear, cpu_bound, io_bound_cold, io_bound_put, mixed")
        sys.exit(1)
    
    command = sys.argv[1]
    
    if command == "clear":
        clear_database()
    elif command == "cpu_bound":
        clear_database()
        populate_cpu_bound()
    elif command == "io_bound_cold":
        clear_database()
        populate_io_bound_cold()
    elif command == "io_bound_put":
        clear_database()
        populate_io_bound_put()
    elif command == "mixed":
        clear_database()
        populate_mixed()
    else:
        print(f"Unknown command: {command}")
        sys.exit(1)

if __name__ == "__main__":
    main()
