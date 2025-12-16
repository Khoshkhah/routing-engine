#!/bin/bash
# Build the C++ routing engine
set -e

cd "$(dirname "$0")/.."

if [ -z "$CONDA_PREFIX" ]; then
    echo "Error: Conda environment not activated. Run 'conda activate routing-engine' first."
    exit 1
fi

echo "Building routing engine..."

cd cpp
mkdir -p build
cd build

cmake .. \
    -DCMAKE_PREFIX_PATH="$CONDA_PREFIX" \
    -DCMAKE_BUILD_TYPE=Release \
    -G Ninja

ninja

echo ""
echo "Build complete! Binary at: cpp/build/routing_engine"
