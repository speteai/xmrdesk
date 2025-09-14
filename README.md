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

## Download
* **[Binary releases](https://github.com/xmrig/xmrig/releases)**
* **[Build from source](https://xmrig.com/docs/miner/build)**

## Usage
The preferred way to configure the miner is the [JSON config file](https://xmrig.com/docs/miner/config) as it is more flexible and human friendly. The [command line interface](https://xmrig.com/docs/miner/command-line-options) does not cover all features, such as mining profiles for different algorithms. Important options can be changed during runtime without miner restart by editing the config file or executing [API](https://xmrig.com/docs/miner/api) calls.

* **[Wizard](https://xmrig.com/wizard)** helps you create initial configuration for the miner.
* **[Workers](http://workers.xmrig.info)** helps manage your miners via HTTP API.

## Donations
* Default donation 1% (1 minute in 100 minutes) can be increased via option `donate-level` or disabled in source code.
* XMR: `48edfHu7V9Z84YzzMa6fUueoELZ9ZRXq9VetWzYGzKt52XU5xvqgzYnDK9URnRoJMk1j8nLwEVsaSWJ4fhdUyZijBGUicoD`

## Developers
* **[xmrig](https://github.com/xmrig)**
* **[sech1](https://github.com/SChernykh)**

## Contacts
* support@xmrig.com
* [reddit](https://www.reddit.com/user/XMRig/)
* [twitter](https://twitter.com/xmrig_dev)
