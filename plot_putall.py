#!/usr/bin/env python3
"""
Plot throughput, average latency, and CPU utilization versus thread count from results_putall.csv.
"""

import csv
import os
from pathlib import Path

_mpl_dir = Path(".matplotlib")
_mpl_dir.mkdir(exist_ok=True)
os.environ.setdefault("MPLCONFIGDIR", str(_mpl_dir))

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def read_results(path: Path):
    rows = []
    with path.open() as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                rows.append(
                    {
                        "threads": int(row["threads"]),
                        "throughput": float(row["throughput_req_per_s"]),
                        "lat_avg": float(row["lat_avg_ms"]),
                        "cpu": float(row["cpu_avg_pct"]),
                    }
                )
            except (KeyError, ValueError):
                continue
    rows.sort(key=lambda r: r["threads"])
    return rows


def plot_throughput(rows, out_path: Path):
    threads = [r["threads"] for r in rows]
    thr = [r["throughput"] for r in rows]
    max_thr = max(thr)
    plateau_thresh = max_thr * 0.98  # 98% of peak
    plateau_start = None
    for t, v in zip(threads, thr):
        if v >= plateau_thresh:
            plateau_start = t
            break

    plt.figure(figsize=(6, 4))
    plt.plot(threads, thr, marker="o", label="throughput")
    plt.xlabel("Threads")
    plt.ylabel("Throughput (req/s)")
    plt.title("putall: Throughput vs Threads")
    plt.grid(True, linestyle="--", alpha=0.4)
    if plateau_start is not None:
        plt.axvspan(plateau_start, threads[-1], color="tab:green", alpha=0.12, label="plateau")
        plt.axhline(plateau_thresh, color="tab:green", linestyle="--", alpha=0.5)
        plt.text(plateau_start, plateau_thresh * 1.02, "plateau ≥98% peak", color="tab:green")
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_path)
    plt.close()


def plot_latency(rows, out_path: Path):
    threads = [r["threads"] for r in rows]
    avg_latency = [r["lat_avg"] for r in rows]
    plt.figure(figsize=(7, 4))
    plt.plot(threads, avg_latency, marker="o", label="avg")
    plt.xlabel("Threads")
    plt.ylabel("Latency (ms)")
    plt.title("putall: Average Latency vs Threads")
    plt.grid(True, linestyle="--", alpha=0.4)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_path)
    plt.close()


def plot_cpu(rows, out_path: Path):
    threads = [r["threads"] for r in rows]
    cpu = [r["cpu"] for r in rows]
    plt.figure(figsize=(6, 4))
    plt.plot(threads, cpu, marker="o", color="tab:red")
    plt.xlabel("Threads")
    plt.ylabel("CPU Utilization (%)")
    plt.title("putall: CPU vs Threads")
    plt.grid(True, linestyle="--", alpha=0.4)
    plt.tight_layout()
    plt.savefig(out_path)
    plt.close()


def main():
    csv_path = Path("results_putall.csv")
    if not csv_path.exists():
        raise SystemExit("results_putall.csv not found")
    rows = read_results(csv_path)
    if not rows:
        raise SystemExit("No rows found in results_putall.csv")

    plot_throughput(rows, Path("plot_throughput.png"))
    plot_latency(rows, Path("plot_latency.png"))
    plot_cpu(rows, Path("plot_cpu.png"))
    print("Wrote plots: plot_throughput.png, plot_latency.png, plot_cpu.png")


if __name__ == "__main__":
    main()
