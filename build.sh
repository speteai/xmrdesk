#!/bin/bash

# XMRDesk Build Script

set -e

echo "Building XMRDesk..."

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DWITH_QT_GUI=ON \
    -DWITH_HWLOC=OFF \
    -DWITH_OPENCL=OFF \
    -DWITH_CUDA=OFF \
    -DWITH_RANDOMX=ON \
    -DWITH_HTTP=ON

# Build
make -j$(nproc)

echo "Build completed successfully!"
echo "Executable: $(pwd)/xmrdesk"