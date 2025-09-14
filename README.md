# XMRDesk

[![GitHub license](https://img.shields.io/github/license/speteai/xmrdesk.svg)](https://github.com/speteai/xmrdesk/blob/master/LICENSE)
[![GitHub stars](https://img.shields.io/github/stars/speteai/xmrdesk.svg)](https://github.com/speteai/xmrdesk/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/speteai/xmrdesk.svg)](https://github.com/speteai/xmrdesk/network)

XMRDesk is a high performance, open source Monero (XMR) miner with an attractive Qt-based GUI. Based on XMRig, it provides real-time hashrate monitoring, CPU temperature display, and easy pool configuration for popular mining pools. Designed specifically for desktop users who want a user-friendly mining experience.

## Features
- **Qt-based GUI** with real-time monitoring
- **Hashrate visualization** with historical charts
- **CPU temperature monitoring**
- **Pre-configured pools**: supportxmr.com, qubic.org, nanopool.org
- **CPU mining** (x86/x64/ARMv7/ARMv8) optimized for XMR

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
