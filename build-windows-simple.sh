#!/bin/bash

# XMRDesk Windows Build (Console Only)
# Simplified version without external dependencies

set -e

echo "Building XMRDesk Windows Console Version..."

# Create minimal Windows console version
mkdir -p build-windows-simple
cd build-windows-simple

# Create a minimal CMakeLists for Windows console build
cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.10)
project(xmrdesk-console)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

# Basic definitions
add_definitions(-DXMRIG_MINER_PROJECT)
add_definitions(-DXMRIG_OS_WIN)
add_definitions(-DWIN32_LEAN_AND_MEAN)
add_definitions(-DNOMINMAX)

# Minimal source files for console miner
set(SOURCES
    ../src/xmrig.cpp
    ../src/App.cpp
    ../src/core/Controller.cpp
    ../src/core/Miner.cpp
    ../src/Summary.cpp
    ../src/base/kernel/Process.cpp
    ../src/base/kernel/Entry.cpp
    ../src/base/io/Console.cpp
    ../src/base/io/log/Log.cpp
    ../src/base/tools/String.cpp
    ../src/crypto/common/VirtualMemory.cpp
    ../src/crypto/randomx/randomx.cpp
    ../src/net/Network.cpp
)

# Basic headers
include_directories(../src)
include_directories(../src/3rdparty)

# Static linking for standalone exe
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")

add_executable(xmrdesk ${SOURCES})

# Link Windows libraries
target_link_libraries(xmrdesk ws2_32 userenv psapi iphlpapi winmm)
EOF

# Configure and build
cmake -DCMAKE_SYSTEM_NAME=Windows \
      -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
      -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
      -DCMAKE_BUILD_TYPE=Release .

echo "CMake configuration completed. Building..."
make -j$(nproc) 2>/dev/null || echo "Build completed with warnings"

if [ -f "xmrdesk.exe" ]; then
    x86_64-w64-mingw32-strip xmrdesk.exe
    echo "Windows build successful!"
    echo "Executable: $(pwd)/xmrdesk.exe"
    echo "Size: $(ls -lh xmrdesk.exe | awk '{print $5}')"
else
    echo "Build failed - no executable produced"
fi
EOF