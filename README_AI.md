# README_AI.md

## stratum-proxy - Universal Stratum Proxy

This project is a universal stratum proxy that works with any Bitcoin/Litecoin-forked coin core.
It uses the actual hash function from the selected coin core, ensuring compatibility with any
PoW algorithm changes in the upstream coin repository.

## Tested Coin Cores

| Coin | Version | PoW Algorithm | Build System | Hash Function |
|------|---------|---------------|--------------|---------------|
| Bitcoin Core | v28.0 | SHA256d | CMake | GetHash |
| Bitcoin II | main | SHA256d | CMake | GetHash |
| Fixedcoin | v29 | SHA256d | CMake | GetHash |
| Rincoin | v1.0.2 | RinHash | Autotools | GetPoWHash |
| Yenten | yenten-6 | YescryptR16 | Autotools | GetPoWHash |

Sample configurations available in `coin_core_samples/`.

## Architecture

The proxy consists of:
1. **Main proxy code** (`src/main.cpp`) - Handles stratum protocol, connection management, share evaluation
2. **Coin hash wrapper** (`src/coin_hash_wrapper.cpp`) - Interfaces with the coin core's hash function
3. **Coin core** (`coin_core/`) - The actual coin core (Bitcoin, Litecoin, Rincoin, etc.) cloned and built

## Automated Coin Core Integration Steps

1. **Configuration**
   - Edit `coin_core_repo.conf` and set `COIN_CORE_REPO_URL` to the git repository URL of the desired coin core.
   - Examples:
     - Bitcoin: `https://github.com/bitcoin/bitcoin.git`
     - Litecoin: `https://github.com/litecoin-project/litecoin.git`
     - Rincoin: `https://github.com/AstaFrode/rincoin.git`

2. **Cloning the Coin Core**
   - Run `./scripts/clone_coin_core.sh` or `make clone-core`
   - This script reads the URL from `coin_core_repo.conf`, removes any existing `coin_core` directory,
     and clones the specified repository into `coin_core`.

3. **Building the Coin Core**
   - Run `./scripts/build_coin_core.sh` or `make build-core`
   - This script detects the build system (autotools or CMake) and builds the core in the `coin_core`
     directory, disabling unnecessary components (GUI, wallet, tests, bench) for faster builds.
   - **Note:** This may take 10-30 minutes depending on your system.

4. **Building stratum-proxy**
   - Run `make build` or:
     ```bash
     mkdir -p build && cd build
     cmake ..
     make -j$(nproc)
     ```
   - The resulting binary will be at `build/stratum-proxy`.

## Directory Structure

```
stratum-proxy/
├── coin_core/              # Cloned and built coin core (gitignored)
├── coin_core_repo.conf     # Configuration file for coin core
├── coin_core_samples/      # Sample configurations for tested coins
├── scripts/
│   ├── clone_coin_core.sh  # Script to clone coin core
│   └── build_coin_core.sh  # Script to build coin core
├── src/
│   ├── main.cpp            # Main proxy implementation
│   ├── coin_hash_wrapper.h # Header for coin hash interface
│   └── coin_hash_wrapper.cpp # Implementation linking to coin core
├── include/                # Header files (endian, hex utilities)
├── CMakeLists.txt          # CMake build configuration
├── Makefile                # Convenience makefile
├── COIN_CORES.md           # Coin configuration documentation & FAQ
└── README_AI.md            # This file
```

## Configuration Options (coin_core_repo.conf)

| Setting | Required | Description |
|---------|----------|-------------|
| `COIN_CORE_REPO_URL` | Yes | Git repository URL |
| `COIN_CORE_BRANCH` | No | Branch, tag, or commit to checkout |
| `COIN_CORE_CXX_STANDARD` | No | C++ standard (default: 20, older forks need 17) |
| `COIN_CORE_HASH_FUNCTION` | Yes | Method name on `CBlockHeader` (e.g., `GetHash`, `GetPoWHash`) |
| `COIN_CORE_LIB_DIR` | Yes | Path to .a library files (relative to coin_core/) |
| `COIN_CORE_LIB_PREFIX` | Yes | Library name prefix (e.g., `bitcoin`, `fixedcoin`) |
| `COIN_CORE_CONFIGURE_OPTS` | No | Configure/cmake options |
| `COIN_CORE_EXTRA_INCLUDES` | No | Extra include paths (relative to coin_core/src) |
| `COIN_CORE_EXTRA_LIBS` | No | Extra libraries to link |

## How It Works

1. The proxy receives block header data from the pool via stratum protocol.
2. When a miner submits a share, the proxy reconstructs the block header.
3. The proxy calls `CoinHash::computeBlockHash()` which internally uses the coin core's
   hash function (configured via `COIN_CORE_HASH_FUNCTION`).
4. The hash is evaluated against the pool's difficulty target.
5. The share is forwarded to the pool.

## Key Benefits

- **Universal Compatibility:** Works with any Bitcoin/Litecoin fork without code changes.
- **Explicit Configuration:** All coin-specific settings are in `coin_core_repo.conf`, no guessing.
- **Always Up-to-Date:** If the upstream coin changes its PoW algorithm, rebuild to get updates.
- **No Manual Maintenance:** No need to manually extract or port hash functions from the coin core.

## Dependencies

- CMake 3.16+
- C++20 compiler (GCC 10+, Clang 13+)
- OpenSSL development libraries
- Boost development libraries (including filesystem)
- Build tools for the coin core (autotools or cmake)

## Usage

```bash
./build/stratum-proxy stratum.toml
```

See `stratum-sample.toml` for configuration options.

---
These steps ensure that stratum-proxy can be built for any Bitcoin/Litecoin forked coin by simply
configuring `coin_core_repo.conf` and running the provided scripts.

