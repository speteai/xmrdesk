# 🚀 XMRDesk - Windows GUI Mining

[![GitHub license](https://img.shields.io/github/license/speteai/xmrdesk.svg)](https://github.com/speteai/xmrdesk/blob/master/LICENSE)
[![GitHub stars](https://img.shields.io/github/stars/speteai/xmrdesk.svg)](https://github.com/speteai/xmrdesk/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/speteai/xmrdesk.svg)](https://github.com/speteai/xmrdesk/network)

**XMRDesk** is a high-performance, open-source Monero (XMR) miner with a beautiful **Qt6 GUI** designed specifically for **Windows desktop users**. Based on XMRig, it provides an intuitive interface with real-time monitoring, making Monero mining accessible to everyone.

## 🎯 **Windows-First Design**
Built with Windows users in mind - no command-line knowledge required!

## ✨ Features
- 🖥️ **Beautiful Qt6 GUI** - Modern Windows interface
- 📊 **Real-time hashrate charts** - 60-second live visualization
- 🌡️ **CPU temperature monitoring** - Color-coded thermal protection
- ⚡ **One-click pool selection** - SupportXMR, Qubic, Nanopool pre-configured
- 🎮 **Drag & drop wallet setup** - No typing complex addresses
- 🔥 **Optimized for Windows** - High-performance CPU mining
- 📱 **Professional interface** - Clean, intuitive design

## 🏃‍♂️ Quick Start (Windows)
1. **Download:** [Latest Release](https://github.com/speteai/xmrdesk/releases)
2. **Extract** the ZIP file anywhere
3. **Double-click** `start-gui.bat`
4. **Enter** your XMR wallet address
5. **Click** "Start Mining" - that's it! 🎉

## Screenshots

![XMRDesk GUI](docs/screenshot.png)

## Build from Source

### Prerequisites
- Qt6 (Core, Widgets, Charts)
- CMake 3.10+
- GCC/Clang compiler
- libuv development package

### Ubuntu/Debian
```bash
sudo apt update
sudo apt install build-essential cmake libuv1-dev qt6-base-dev qt6-charts-dev
git clone https://github.com/speteai/xmrdesk.git
cd xmrdesk
./build.sh
```

### Usage

**GUI Mode (Default):**
```bash
./build/xmrdesk
```

**Console Mode:**
```bash
./build/xmrdesk --no-gui
```

### Configuration
- **Pool**: Select from pre-configured pools (SupportXMR, Qubic, Nanopool)
- **Wallet**: Enter your XMR wallet address
- **Worker**: Optional worker name for pool identification

## Donations
* Default donation 1% (1 minute in 100 minutes) can be increased via option `donate-level` or disabled in source code.
* XMR: `48ckezCUYfnj3vDtQRtH1X4ExpvowjRBJj7U36P13KbDPeZ6M3Pjuev3xdCkXPuAuQNomuSDXTBZFTkQRfP33t3gMTjJCpL`

## Developers
* **[xmrig](https://github.com/xmrig)**
* **[sech1](https://github.com/SChernykh)**

## Contacts
* support@xmrig.com
* [reddit](https://www.reddit.com/user/XMRig/)
* [twitter](https://twitter.com/xmrig_dev)
