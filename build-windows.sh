#!/bin/bash

# XMRDesk Windows Cross-Compilation Script

set -e

echo "Building XMRDesk for Windows..."

# Create build directory for Windows
mkdir -p build-windows
cd build-windows

# Set up cross-compilation environment
export CC=x86_64-w64-mingw32-gcc
export CXX=x86_64-w64-mingw32-g++
export AR=x86_64-w64-mingw32-ar
export STRIP=x86_64-w64-mingw32-strip

# Configure with CMake for Windows (console version only for now)
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32 \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DWITH_QT_GUI=OFF \
    -DWITH_HWLOC=OFF \
    -DWITH_OPENCL=OFF \
    -DWITH_CUDA=OFF \
    -DWITH_RANDOMX=ON \
    -DWITH_HTTP=ON \
    -DWITH_TLS=OFF \
    -DBUILD_STATIC=ON

# Build
make -j$(nproc)

# Strip the executable to reduce size
x86_64-w64-mingw32-strip xmrdesk.exe

echo "Windows build completed successfully!"
echo "Executable: $(pwd)/xmrdesk.exe"
echo "Size: $(ls -lh xmrdesk.exe | awk '{print $5}')"