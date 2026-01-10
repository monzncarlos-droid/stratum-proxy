# stratum-proxy

Universal Stratum Proxy for Bitcoin/Litecoin-forked Coins.

## Features

- Works with **any** Bitcoin or Litecoin fork
- Uses the coin core's actual hash function (not a reimplementation)
- Automatically detects `GetPoWHash()` (altcoins) vs `GetHash()` (Bitcoin) at compile time
- TOML-based configuration with regex decorators for log coloring
- Zero manual hash function porting required

## Quick Start

```bash
# 1. Configure the coin core (see COIN_CORES.md for examples)
cat > coin_core_repo.conf << 'EOF'
COIN_CORE_REPO_URL=https://github.com/bitcoin/bitcoin.git
COIN_CORE_BRANCH=v28.0
COIN_CORE_CONFIGURE_OPTS=--disable-wallet --disable-tests --disable-bench --disable-zmq --without-gui
EOF

# 2. Clone and build the coin core
make clone-core
make build-core

# 3. Build the proxy
make build

# 4. Configure and run
cp stratum-sample.toml stratum.toml
# Edit stratum.toml with your pool settings
./build/stratum-proxy stratum.toml
```

## How It Works

1. You specify the git URL of your target coin's core repository
2. The build system clones and compiles the coin core's consensus library
3. stratum-proxy links against the coin core's libraries
4. Block header hashing uses the coin core's native hash function:
   - `CBlockHeader::GetPoWHash()` for most altcoins (Litecoin, Rincoin, etc.)
   - `CBlockHeader::GetHash()` for Bitcoin and similar forks

This means any PoW algorithm changes pushed to the upstream coin repo are automatically
available after rebuilding.

## Documentation

- [BUILD.md](BUILD.md) - Detailed build instructions
- [COIN_CORES.md](COIN_CORES.md) - Ready-to-use configurations for various coins
- [README_AI.md](README_AI.md) - Technical documentation and architecture

## Tested Coins

| Coin | Version | PoW Algorithm | Build System |
|------|---------|---------------|---------------|
| Bitcoin Core | v28.0 | SHA256d | CMake |
| Bitcoin II | main | SHA256d | CMake |
| Fixedcoin | v29 | SHA256d | CMake |
| Rincoin | v1.0.2 | RinHash | Autotools |
| Yenten | yenten-6 | YescryptR16 | Autotools |

## License

See [LICENSE](LICENSE) for details.

