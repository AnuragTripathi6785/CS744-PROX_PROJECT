#!/usr/bin/env python3
"""
Wrapper to run only the IO-bound PUT-heavy experiments.
Usage: python3 run_io_experiment.py
      (passes through any extra args to run_experiments.py if needed)
"""

import subprocess
import sys


def main():
    cmd = ["python3", "run_experiments.py", "io"] + sys.argv[1:]
    subprocess.run(cmd, check=True)


if __name__ == "__main__":
    main()

