#!/bin/bash
# Setup script for virtual environment (alternative to system-wide install)

echo "Setting up virtual environment for experiments..."

# Create virtual environment
python3 -m venv venv

# Activate virtual environment
source venv/bin/activate

# Install dependencies
pip install psutil matplotlib numpy

echo ""
echo "Virtual environment setup complete!"
echo ""
echo "To use it, run:"
echo "  source venv/bin/activate"
echo "  python3 run_experiments.py"
echo "  python3 generate_graphs.py"
echo ""
echo "To deactivate:"
echo "  deactivate"

