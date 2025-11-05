#!/bin/bash
# Linux/Mac shell script to run DXF validation
# Usage: ./run_validation.sh file.dxf

if [ $# -eq 0 ]; then
    echo "Usage: ./run_validation.sh file.dxf"
    echo ""
    echo "Example: ./run_validation.sh /path/to/your/file.dxf"
    exit 1
fi

echo "Installing dependencies..."
pip install -r requirements.txt

echo ""
echo "Validating DXF file: $1"
echo ""

python validate_dxf.py -v "$1"

echo ""
echo "Validation complete."

