# Windows Build Instructions for XMRDesk

## Option 1: Automatic GitHub Actions Build

The easiest way to get a Windows executable is to use our automated build system:

1. Go to: https://github.com/speteai/xmrdesk/actions
2. Click on "Build XMRDesk" workflow
3. Click "Run workflow" button
4. Download the `xmrdesk-windows-x64` artifact

## Option 2: Manual Windows Build

### Prerequisites
- Windows 10/11
- Visual Studio 2022 Community Edition
- CMake 3.10+
- vcpkg package manager

### Installation Steps

1. **Install Visual Studio 2022**
   - Download from: https://visualstudio.microsoft.com/downloads/
   - Install with "Desktop development with C++" workload

2. **Install vcpkg**
   ```cmd
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   .\vcpkg integrate install
   ```

3. **Install dependencies**
   ```cmd
   .\vcpkg install libuv:x64-windows-static
   ```

4. **Build XMRDesk**
   ```cmd
   git clone https://github.com/speteai/xmrdesk.git
   cd xmrdesk
   mkdir build
   cd build

   cmake .. -G "Visual Studio 17 2022" -A x64 ^
     -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
     -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
     -DWITH_QT_GUI=OFF ^
     -DWITH_HWLOC=OFF ^
     -DWITH_OPENCL=OFF ^
     -DWITH_CUDA=OFF ^
     -DWITH_RANDOMX=ON ^
     -DWITH_HTTP=ON ^
     -DBUILD_STATIC=ON

   cmake --build . --config Release
   ```

5. **The executable will be at:** `build/Release/xmrdesk.exe`

## Option 3: Use GitHub Release

Check the [Releases page](https://github.com/speteai/xmrdesk/releases) for pre-built Windows executables.

## Running XMRDesk on Windows

1. Download the executable
2. Create a batch file to run it easily:
   ```cmd
   @echo off
   echo XMRDesk - Monero Mining
   echo.
   echo Configure your wallet address before mining!
   echo.
   pause
   xmrdesk.exe --donate-level=1
   ```

3. Configure your mining settings when prompted
4. Start mining!

## Notes

- The Windows version currently runs in console mode
- GUI version will be available in future releases
- Make sure to configure your XMR wallet address
- Default donation level is 1% (supports development)