# BUILD.md - Build Instructions for stratum-proxy

## Overview

stratum-proxy is a universal stratum proxy that works with any Bitcoin/Litecoin-forked coin core.
It dynamically links to the coin core's hash function, ensuring compatibility with any PoW algorithm.

## Prerequisites

- Linux (tested on Ubuntu/Debian)
- CMake 3.16+
- C++17 or C++20 compatible compiler (GCC 7+ or Clang 5+)
  - Modern coin cores (Bitcoin v28+, Fixedcoin) require C++20
  - Older forks (Yenten, Rincoin) require C++17
  - Set `COIN_CORE_CXX_STANDARD` in your config (default: 20)
- OpenSSL development libraries
- Boost development libraries (including filesystem)
- Git

### Install dependencies on Ubuntu/Debian:

```bash
sudo apt update
sudo apt install build-essential cmake git libssl-dev libboost-all-dev
sudo apt install autoconf automake libtool pkg-config bsdmainutils libunivalue-dev
```
These should be enough for successful build. Other packages can be needed for a specific coin core.

## Quick Start

1. **Configure the coin core repository:**
   ```bash
   # Edit coin_core_repo.conf - see COIN_CORES.md for ready-to-use configurations
   # Example for Bitcoin Core:
   cat > coin_core_repo.conf << 'EOF'
   COIN_CORE_REPO_URL=https://github.com/bitcoin/bitcoin.git
   COIN_CORE_BRANCH=v28.0
   COIN_CORE_CONFIGURE_OPTS=--disable-wallet --disable-tests --disable-bench --disable-zmq --without-gui
   EOF
   ```

2. **Clone the coin core:**
   ```bash
   make clone-core
   # Or: ./scripts/clone_coin_core.sh
   ```

3. **Build the coin core:**
   ```bash
   make build-core
   # Or: ./scripts/build_coin_core.sh
   ```
   This step may take 10-30 minutes.

4. **Build stratum-proxy:**
   ```bash
   make build
   ```

5. **Run the proxy:**
   ```bash
   # Copy and edit the sample config
   cp stratum-sample.toml stratum.toml
   # Edit stratum.toml with your pool and port settings
   
   # Binary name depends on COIN_CORE_BINARY_SUFFIX in config
   # e.g., stratum-proxy_rincoin, stratum-proxy_bitcoin28
   ./build/stratum-proxy_<suffix> stratum.toml
   ```

## Manual Build Steps

If you prefer manual steps:

```bash
# 1. Clone coin core
./scripts/clone_coin_core.sh

# 2. Build coin core
./scripts/build_coin_core.sh

# 3. Configure and build stratum-proxy
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

## Switching Coin Cores

To switch to a different coin:

1. Edit `coin_core_repo.conf` with the new repo URL
2. Run `make clone-core` (this will delete the old coin_core)
3. Run `make build-core`
4. Run `make build`

## Troubleshooting

### CMake can't find coin core
Make sure you've run `clone_coin_core.sh` and `build_coin_core.sh` before running cmake.

### Build fails with missing headers
Some coin cores may have additional dependencies. Check the coin core's documentation.

### Linking errors
Ensure the coin core built successfully. Check `coin_core/` for build artifacts.

## Supported Coins

Any Bitcoin/Litecoin fork that follows the standard structure should work:
- **Bitcoin Core** (v28.0 tested) - uses `GetHash()`
- **Litecoin Core** - uses `GetPoWHash()` with Scrypt
- **Rincoin** - uses `GetPoWHash()` with RinHash (BLAKE3→Argon2d→SHA3-256)
- Other forks with `CBlockHeader::GetPoWHash()` or `CBlockHeader::GetHash()`

See [COIN_CORES.md](COIN_CORES.md) for ready-to-use configurations.

