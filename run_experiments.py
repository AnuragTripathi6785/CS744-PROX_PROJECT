#!/usr/bin/env python3
"""
Comprehensive Load Testing Framework for CS744 Phase 2
Runs experiments at multiple load levels, collects metrics, and generates graphs.
"""

import subprocess
import re
import time
import csv
import json
import os
import sys
import psutil
import threading
from datetime import datetime
from collections import defaultdict

# Configuration
SERVER_URL = "http://localhost:8080"
DURATION = 180  # 3 minutes per test (seconds)
# Explicit (threads, connections) pairs for each run
LOAD_MATRIX = [
    (2, 200),
    (4, 200),
    (8, 200),
    (12, 200),
    (24, 200),
    (36, 200),
]
OUTPUT_DIR = "experiment_results"
RESULTS_CSV = os.path.join(OUTPUT_DIR, "results.csv")
METRICS_JSON = os.path.join(OUTPUT_DIR, "metrics.json")

# CPU pinning: Load generator should run on different CPU than server
# On macOS, we'll use taskset or cpuset if available
LOADGEN_CPU = None  # Will be set based on system
SERVER_CPU = None

class MetricsCollector:
    """Collects system metrics during load testing"""
    
    def __init__(self, duration, interval=1):
        self.duration = duration
        self.interval = interval
        self.running = False
        self.metrics = {
            'cpu_percent': [],
            'cpu_times': [],
            'disk_io': [],
            'memory': [],
            'timestamps': []
        }
        self.process = None
        self.server_pid = None
        
    def find_server_process(self):
        """Find the proxy_server process"""
        for proc in psutil.process_iter(['pid', 'name', 'cmdline']):
            try:
                if 'proxy_server' in proc.info['name'] or \
                   (proc.info['cmdline'] and any('proxy_server' in str(cmd) for cmd in proc.info['cmdline'])):
                    self.server_pid = proc.info['pid']
                    self.process = psutil.Process(self.server_pid)
                    print(f"[METRICS] Found server process PID: {self.server_pid}")
                    return True
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        return False
    
    def collect(self):
        """Collect metrics in a separate thread"""
        if not self.find_server_process():
            print("[WARNING] Could not find server process, collecting system-wide metrics")
            self.process = None
        
        start_time = time.time()
        while self.running and (time.time() - start_time) < self.duration:
            try:
                timestamp = time.time()
                
                if self.process:
                    # Per-process metrics
                    cpu_percent = self.process.cpu_percent(interval=None)
                    cpu_times = self.process.cpu_times()
                    memory = self.process.memory_info()
                    try:
                        disk_io = self.process.io_counters()
                    except (AttributeError, psutil.AccessDenied):
                        disk_io = None
                else:
                    # System-wide metrics
                    cpu_percent = psutil.cpu_percent(interval=None)
                    cpu_times = psutil.cpu_times()
                    memory = psutil.virtual_memory()
                    disk_io = psutil.disk_io_counters()
                
                self.metrics['cpu_percent'].append(cpu_percent)
                self.metrics['cpu_times'].append(cpu_times)
                self.metrics['memory'].append(memory)
                if disk_io:
                    self.metrics['disk_io'].append(disk_io)
                self.metrics['timestamps'].append(timestamp)
                
            except Exception as e:
                print(f"[WARNING] Error collecting metrics: {e}")
            
            time.sleep(self.interval)
    
    def start(self):
        """Start collecting metrics"""
        self.running = True
        self.collector_thread = threading.Thread(target=self.collect, daemon=True)
        self.collector_thread.start()
    
    def stop(self):
        """Stop collecting metrics"""
        self.running = False
        if hasattr(self, 'collector_thread'):
            self.collector_thread.join(timeout=2)
    
    def get_averages(self):
        """Get average metrics"""
        if not self.metrics['cpu_percent']:
            return {}
        
        avg_cpu = sum(self.metrics['cpu_percent']) / len(self.metrics['cpu_percent'])
        
        # Calculate disk I/O rates
        disk_read_rate = 0
        disk_write_rate = 0
        if self.metrics['disk_io'] and len(self.metrics['disk_io']) > 1:
            first_io = self.metrics['disk_io'][0]
            last_io = self.metrics['disk_io'][-1]
            time_diff = self.metrics['timestamps'][-1] - self.metrics['timestamps'][0]
            if time_diff > 0:
                if hasattr(first_io, 'read_bytes'):
                    disk_read_rate = (last_io.read_bytes - first_io.read_bytes) / time_diff
                    disk_write_rate = (last_io.write_bytes - first_io.write_bytes) / time_diff
        
        return {
            'avg_cpu_percent': avg_cpu,
            'max_cpu_percent': max(self.metrics['cpu_percent']) if self.metrics['cpu_percent'] else 0,
            'disk_read_rate_mbps': disk_read_rate / (1024 * 1024),
            'disk_write_rate_mbps': disk_write_rate / (1024 * 1024),
        }

def parse_wrk_output(output):
    """Parse wrk output to extract metrics"""
    metrics = {
        'throughput': 0,
        'latency_mean': 0,
        'latency_p50': 0,
        'latency_p99': 0,
        'requests': 0,
        'errors': 0
    }
    
    # Parse throughput
    req_match = re.search(r"Requests/sec:\s+([\d\.]+)", output)
    if req_match:
        metrics['throughput'] = float(req_match.group(1))
    
    # Parse latency (mean)
    lat_match = re.search(r"Latency\s+([\d\.]+)(\w+)", output)
    if lat_match:
        val = float(lat_match.group(1))
        unit = lat_match.group(2)
        if unit == "us":
            metrics['latency_mean'] = val / 1000.0
        elif unit == "ms":
            metrics['latency_mean'] = val
        elif unit == "s":
            metrics['latency_mean'] = val * 1000.0
    
    # Parse latency percentiles (if --latency flag was used)
    p50_match = re.search(r"50%\s+([\d\.]+)(\w+)", output)
    if p50_match:
        val = float(p50_match.group(1))
        unit = p50_match.group(2)
        if unit == "us":
            metrics['latency_p50'] = val / 1000.0
        elif unit == "ms":
            metrics['latency_p50'] = val
        elif unit == "s":
            metrics['latency_p50'] = val * 1000.0
    
    p99_match = re.search(r"99%\s+([\d\.]+)(\w+)", output)
    if p99_match:
        val = float(p99_match.group(1))
        unit = p99_match.group(2)
        if unit == "us":
            metrics['latency_p99'] = val / 1000.0
        elif unit == "ms":
            metrics['latency_p99'] = val
        elif unit == "s":
            metrics['latency_p99'] = val * 1000.0
    
    # Parse total requests
    req_total_match = re.search(r"(\d+)\s+requests", output)
    if req_total_match:
        metrics['requests'] = int(req_total_match.group(1))
    
    # Parse errors
    error_match = re.search(r"Socket errors:.*read\s+(\d+)", output)
    if error_match:
        metrics['errors'] = int(error_match.group(1))
    
    return metrics

def warm_cache_for_cpu_workload(repetitions=3):
    """
    Prime the hot-cache workload so CPU is the clear bottleneck from the first second.
    Sends a few GETs for the 10 hot keys before each CPU-bound run.
    """
    print("[WARMUP] Priming cache for CPU-bound workload...")
    for _ in range(repetitions):
        for i in range(1, 11):
            subprocess.run(
                ["curl", "-s", f"{SERVER_URL}/hot{i}"],
                capture_output=True
            )
    time.sleep(1)

def run_load_test(workload_name, script, connections, threads=None):
    """Run a single load test and collect metrics"""
    if threads is None:
        threads = min(connections, 8)
    
    print(f"\n{'='*60}")
    print(f"Running: {workload_name}")
    print(f"Connections: {connections}, Threads: {threads}, Duration: {DURATION}s")
    print(f"{'='*60}")
    
    # Start metrics collection
    collector = MetricsCollector(DURATION, interval=1)
    collector.start()
    
    # Wait a bit for metrics to stabilize
    time.sleep(2)
    
    # Run wrk
    cmd = [
        "wrk",
        f"-t{threads}",
        f"-c{connections}",
        f"-d{DURATION}s",
        "-s", script,
        "--latency",  # Enable latency distribution
        SERVER_URL
    ]
    
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=DURATION + 60  # Add buffer
        )
        
        output = result.stdout + result.stderr
        print(output)
        
        # Stop metrics collection
        collector.stop()
        
        # Parse results
        wrk_metrics = parse_wrk_output(output)
        sys_metrics = collector.get_averages()
        
        # Combine metrics
        combined = {
            'workload': workload_name,
            'connections': connections,
            'threads': threads,
            **wrk_metrics,
            **sys_metrics
        }
        
        return combined
        
    except subprocess.TimeoutExpired:
        print(f"[ERROR] Test timed out after {DURATION + 60}s")
        collector.stop()
        return None
    except Exception as e:
        print(f"[ERROR] Test failed: {e}")
        collector.stop()
        return None

def setup_workload(workload_name):
    """Setup data for a workload - clears DB and populates only what's needed"""
    print(f"\n[SETUP] Preparing data for {workload_name}...")
    
    # Use db_utils.py to clear and populate
    if workload_name == "CPU_Bound":
        subprocess.run(
            ["python3", "db_utils.py", "cpu_bound"],
            check=True
        )
    elif workload_name == "IO_Bound_Cold":
        subprocess.run(
            ["python3", "db_utils.py", "io_bound_cold"],
            check=True
        )
    elif workload_name == "IO_Bound_Put":
        subprocess.run(
            ["python3", "db_utils.py", "io_bound_put"],
            check=True
        )
    elif workload_name == "Mixed":
        subprocess.run(
            ["python3", "db_utils.py", "mixed"],
            check=True
        )
    
    time.sleep(2)  # Brief pause after setup

def run_workload_experiments(workload_name, script):
    """Run experiments for a workload at all load levels"""
    print(f"\n{'#'*60}")
    print(f"# Workload: {workload_name}")
    print(f"{'#'*60}")
    
    # Setup workload data
    setup_workload(workload_name)
    if workload_name == "CPU_Bound":
        warm_cache_for_cpu_workload()
    
    results = []
    for threads, connections in LOAD_MATRIX:
        if workload_name == "CPU_Bound":
            warm_cache_for_cpu_workload()

        result = run_load_test(workload_name, script, connections, threads)
        if result:
            results.append(result)
            print(f"\n[RESULT] Throughput: {result['throughput']:.2f} req/s, "
                  f"Latency: {result['latency_mean']:.2f} ms, "
                  f"CPU: {result.get('avg_cpu_percent', 0):.1f}% "
                  f"(threads={threads}, conns={connections})")
        
        # Cooldown between tests
        print(f"\n[COOLDOWN] Waiting 10 seconds before next test...")
        time.sleep(10)
    
    return results

def save_results(all_results, csv_path, json_path):
    """Save results to CSV and JSON"""
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    
    if not all_results:
        print("[WARN] No results to save.")
        return
    
    # Save CSV
    with open(csv_path, 'w', newline='') as f:
        fieldnames = list(all_results[0].keys())
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(all_results)
    
    # Save JSON
    with open(json_path, 'w') as f:
        json.dump(all_results, f, indent=2)
    
    print(f"\n[SAVED] Results saved to {csv_path} and {json_path}")

def main():
    """Main experiment runner"""
    import argparse

    parser = argparse.ArgumentParser(description="Run load experiments")
    parser.add_argument(
        "workload",
        nargs="?",
        choices=["cpu", "io", "all"],
        default="all",
        help="Which workload to run: cpu (hot cache), io (PUT heavy), or all (default)",
    )
    args = parser.parse_args()

    # Choose output filenames per workload
    if args.workload == "cpu":
        csv_path = os.path.join(OUTPUT_DIR, "results_cpu.csv")
        json_path = os.path.join(OUTPUT_DIR, "metrics_cpu.json")
    elif args.workload == "io":
        csv_path = os.path.join(OUTPUT_DIR, "results_io.csv")
        json_path = os.path.join(OUTPUT_DIR, "metrics_io.json")
    else:
        csv_path = RESULTS_CSV
        json_path = METRICS_JSON

    print("="*60)
    print("CS744 Phase 2: Comprehensive Load Testing")
    print("="*60)
    print(f"Server URL: {SERVER_URL}")
    print(f"Duration per test: {DURATION}s (5 minutes)")
    print(f"Load matrix (threads, connections): {LOAD_MATRIX}")
    print(f"Results directory: {OUTPUT_DIR}")
    print(f"Selected workload: {args.workload}")
    print(f"Results file: {csv_path}")
    print("="*60)
    
    # Check if server is running
    try:
        result = subprocess.run(
            ["curl", "-s", f"{SERVER_URL}/__stats"],
            capture_output=True,
            timeout=5
        )
        if result.returncode != 0:
            print(f"[ERROR] Server not responding at {SERVER_URL}")
            print("[ERROR] Please start the server with: make run")
            sys.exit(1)
    except Exception as e:
        print(f"[ERROR] Cannot connect to server: {e}")
        print("[ERROR] Please start the server with: make run")
        sys.exit(1)
    
    all_results = []
    
    if args.workload in ("cpu", "all"):
        results = run_workload_experiments("CPU_Bound", "load_test_cpu_bound.lua")
        all_results.extend(results)
    
    if args.workload in ("io", "all"):
        results = run_workload_experiments("IO_Bound_Put", "load_test_io_bound_put.lua")
        all_results.extend(results)
    
    # Save results
    save_results(all_results, csv_path, json_path)
    
    print("\n" + "="*60)
    print("Experiments completed!")
    print(f"Results saved to: {OUTPUT_DIR}/")
    print("Run 'python3 generate_graphs.py' to generate graphs")
    print("="*60)

if __name__ == "__main__":
    main()
