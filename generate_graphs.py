#!/usr/bin/env python3
"""
Generate graphs from experiment results for CS744 Phase 2 report.
Creates graphs showing throughput, latency, and utilization vs load level.
"""

import csv
import json
import os
import sys
import matplotlib
matplotlib.use('Agg')  # Non-interactive backend
import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict

OUTPUT_DIR = "experiment_results"
RESULTS_CSV = os.path.join(OUTPUT_DIR, "results.csv")
GRAPHS_DIR = os.path.join(OUTPUT_DIR, "graphs")

def load_results():
    """Load results from CSV"""
    results = []
    if not os.path.exists(RESULTS_CSV):
        print(f"[ERROR] Results file not found: {RESULTS_CSV}")
        print("[ERROR] Please run experiments first: python3 run_experiments.py")
        sys.exit(1)
    
    with open(RESULTS_CSV, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            # Convert numeric fields
            for key in ['connections', 'throughput', 'latency_mean', 'latency_p50', 
                       'latency_p99', 'avg_cpu_percent', 'max_cpu_percent',
                       'disk_read_rate_mbps', 'disk_write_rate_mbps']:
                if key in row:
                    try:
                        row[key] = float(row[key]) if row[key] else 0.0
                    except ValueError:
                        row[key] = 0.0
            results.append(row)
    
    return results

def group_by_workload(results):
    """Group results by workload"""
    grouped = defaultdict(list)
    for result in results:
        workload = result['workload']
        grouped[workload].append(result)
    
    # Sort each group by connections
    for workload in grouped:
        grouped[workload].sort(key=lambda x: x['connections'])
    
    return grouped

def plot_throughput(grouped_results):
    """Plot throughput vs load level"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    for workload, results in grouped_results.items():
        connections = [r['connections'] for r in results]
        throughput = [r['throughput'] for r in results]
        
        ax.plot(connections, throughput, marker='o', linewidth=2, 
                markersize=8, label=workload)
    
    ax.set_xlabel('Load Level (Concurrent Connections)', fontsize=12)
    ax.set_ylabel('Throughput (Requests/sec)', fontsize=12)
    ax.set_title('Throughput vs Load Level', fontsize=14, fontweight='bold')
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=10)
    ax.set_xlim(left=0)
    ax.set_ylim(bottom=0)
    
    plt.tight_layout()
    plt.savefig(os.path.join(GRAPHS_DIR, 'throughput_vs_load.png'), dpi=300, bbox_inches='tight')
    print(f"[GRAPH] Saved: throughput_vs_load.png")
    plt.close()

def plot_latency(grouped_results):
    """Plot latency vs load level"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    for workload, results in grouped_results.items():
        connections = [r['connections'] for r in results]
        latency = [r['latency_mean'] for r in results]
        
        ax.plot(connections, latency, marker='s', linewidth=2, 
                markersize=8, label=f"{workload} (Mean)")
    
    ax.set_xlabel('Load Level (Concurrent Connections)', fontsize=12)
    ax.set_ylabel('Latency (ms)', fontsize=12)
    ax.set_title('Latency vs Load Level', fontsize=14, fontweight='bold')
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=10)
    ax.set_xlim(left=0)
    ax.set_ylim(bottom=0)
    
    plt.tight_layout()
    plt.savefig(os.path.join(GRAPHS_DIR, 'latency_vs_load.png'), dpi=300, bbox_inches='tight')
    print(f"[GRAPH] Saved: latency_vs_load.png")
    plt.close()

def plot_cpu_utilization(grouped_results):
    """Plot CPU utilization vs load level"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    for workload, results in grouped_results.items():
        connections = [r['connections'] for r in results]
        cpu = [r.get('avg_cpu_percent', 0) for r in results]
        
        ax.plot(connections, cpu, marker='^', linewidth=2, 
                markersize=8, label=workload)
    
    ax.set_xlabel('Load Level (Concurrent Connections)', fontsize=12)
    ax.set_ylabel('CPU Utilization (%)', fontsize=12)
    ax.set_title('CPU Utilization vs Load Level', fontsize=14, fontweight='bold')
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=10)
    ax.set_xlim(left=0)
    ax.set_ylim(0, 100)
    
    plt.tight_layout()
    plt.savefig(os.path.join(GRAPHS_DIR, 'cpu_utilization_vs_load.png'), dpi=300, bbox_inches='tight')
    print(f"[GRAPH] Saved: cpu_utilization_vs_load.png")
    plt.close()

def plot_disk_io(grouped_results):
    """Plot disk I/O vs load level"""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    for workload, results in grouped_results.items():
        connections = [r['connections'] for r in results]
        read_rate = [r.get('disk_read_rate_mbps', 0) for r in results]
        write_rate = [r.get('disk_write_rate_mbps', 0) for r in results]
        
        ax1.plot(connections, read_rate, marker='o', linewidth=2, 
                markersize=8, label=workload)
        ax2.plot(connections, write_rate, marker='s', linewidth=2, 
                markersize=8, label=workload)
    
    ax1.set_xlabel('Load Level (Concurrent Connections)', fontsize=12)
    ax1.set_ylabel('Disk Read Rate (MB/s)', fontsize=12)
    ax1.set_title('Disk Read Rate vs Load Level', fontsize=14, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    ax1.legend(fontsize=10)
    ax1.set_xlim(left=0)
    ax1.set_ylim(bottom=0)
    
    ax2.set_xlabel('Load Level (Concurrent Connections)', fontsize=12)
    ax2.set_ylabel('Disk Write Rate (MB/s)', fontsize=12)
    ax2.set_title('Disk Write Rate vs Load Level', fontsize=14, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    ax2.legend(fontsize=10)
    ax2.set_xlim(left=0)
    ax2.set_ylim(bottom=0)
    
    plt.tight_layout()
    plt.savefig(os.path.join(GRAPHS_DIR, 'disk_io_vs_load.png'), dpi=300, bbox_inches='tight')
    print(f"[GRAPH] Saved: disk_io_vs_load.png")
    plt.close()

def plot_combined_workload(workload_name, results):
    """Plot all metrics for a single workload"""
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle(f'{workload_name} - Performance Metrics', fontsize=16, fontweight='bold')
    
    connections = [r['connections'] for r in results]
    
    # Throughput
    ax = axes[0, 0]
    throughput = [r['throughput'] for r in results]
    ax.plot(connections, throughput, marker='o', linewidth=2, markersize=8, color='blue')
    ax.set_xlabel('Load Level (Concurrent Connections)')
    ax.set_ylabel('Throughput (Requests/sec)')
    ax.set_title('Throughput')
    ax.grid(True, alpha=0.3)
    ax.set_xlim(left=0)
    ax.set_ylim(bottom=0)
    
    # Latency
    ax = axes[0, 1]
    latency = [r['latency_mean'] for r in results]
    ax.plot(connections, latency, marker='s', linewidth=2, markersize=8, color='red')
    ax.set_xlabel('Load Level (Concurrent Connections)')
    ax.set_ylabel('Latency (ms)')
    ax.set_title('Latency (Mean)')
    ax.grid(True, alpha=0.3)
    ax.set_xlim(left=0)
    ax.set_ylim(bottom=0)
    
    # CPU Utilization
    ax = axes[1, 0]
    cpu = [r.get('avg_cpu_percent', 0) for r in results]
    ax.plot(connections, cpu, marker='^', linewidth=2, markersize=8, color='green')
    ax.set_xlabel('Load Level (Concurrent Connections)')
    ax.set_ylabel('CPU Utilization (%)')
    ax.set_title('CPU Utilization')
    ax.grid(True, alpha=0.3)
    ax.set_xlim(left=0)
    ax.set_ylim(0, 100)
    
    # Disk I/O
    ax = axes[1, 1]
    read_rate = [r.get('disk_read_rate_mbps', 0) for r in results]
    write_rate = [r.get('disk_write_rate_mbps', 0) for r in results]
    ax.plot(connections, read_rate, marker='o', linewidth=2, markersize=8, 
            label='Read', color='purple')
    ax.plot(connections, write_rate, marker='s', linewidth=2, markersize=8, 
            label='Write', color='orange')
    ax.set_xlabel('Load Level (Concurrent Connections)')
    ax.set_ylabel('Disk I/O Rate (MB/s)')
    ax.set_title('Disk I/O')
    ax.grid(True, alpha=0.3)
    ax.legend()
    ax.set_xlim(left=0)
    ax.set_ylim(bottom=0)
    
    plt.tight_layout()
    filename = f'{workload_name.lower()}_combined.png'
    plt.savefig(os.path.join(GRAPHS_DIR, filename), dpi=300, bbox_inches='tight')
    print(f"[GRAPH] Saved: {filename}")
    plt.close()

def main():
    """Generate all graphs"""
    print("="*60)
    print("Generating Graphs from Experiment Results")
    print("="*60)
    
    # Create graphs directory
    os.makedirs(GRAPHS_DIR, exist_ok=True)
    
    # Load results
    results = load_results()
    if not results:
        print("[ERROR] No results found")
        sys.exit(1)
    
    # Group by workload
    grouped = group_by_workload(results)
    
    print(f"\nFound {len(grouped)} workload(s): {list(grouped.keys())}")
    print(f"Total data points: {len(results)}")
    
    # Generate comparison graphs (all workloads together)
    print("\n[1/5] Generating throughput comparison graph...")
    plot_throughput(grouped)
    
    print("\n[2/5] Generating latency comparison graph...")
    plot_latency(grouped)
    
    print("\n[3/5] Generating CPU utilization comparison graph...")
    plot_cpu_utilization(grouped)
    
    print("\n[4/5] Generating disk I/O comparison graph...")
    plot_disk_io(grouped)
    
    # Generate individual workload graphs
    print("\n[5/5] Generating individual workload graphs...")
    for workload_name, workload_results in grouped.items():
        plot_combined_workload(workload_name, workload_results)
    
    print("\n" + "="*60)
    print("Graph generation complete!")
    print(f"Graphs saved to: {GRAPHS_DIR}/")
    print("="*60)

if __name__ == "__main__":
    main()

