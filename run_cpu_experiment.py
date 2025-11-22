#!/usr/bin/env python3
"""
Wrapper to run only the CPU-bound (hot cache GET) experiments.
Usage: python3 run_cpu_experiment.py
      (passes through any extra args to run_experiments.py if needed)
"""

import subprocess
import sys


def main():
    cmd = ["python3", "run_experiments.py", "cpu"] + sys.argv[1:]
    subprocess.run(cmd, check=True)


if __name__ == "__main__":
    main()

