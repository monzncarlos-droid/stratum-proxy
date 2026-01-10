# Coin Core Configurations

This document explains how to configure `coin_core_repo.conf` for different Bitcoin/Litecoin-forked coins.

## Sample Configurations

Ready-to-use configuration files are available in the `coin_core_samples/` directory:

| File | Coin | Build System | PoW Algorithm |
|------|------|--------------|---------------|
| `coin_core_repo_bitcoin_28.conf` | Bitcoin Core v28.0 | CMake | SHA256d |
| `coin_core_repo_bitcoinII_main.conf` | Bitcoin II (main) | CMake | SHA256d |
| `coin_core_repo_fixedcoin_v29.conf` | Fixedcoin v29 | CMake | SHA256d |
| `coin_core_repo_rincoin_takologi102.conf` | Rincoin v1.0.2 | Autotools | RinHash |
| `coin_core_repo_yenten_yenten6.conf` | Yenten (yenten-6) | Autotools | YescryptR16 |

To use a sample configuration:

```bash
cp coin_core_samples/coin_core_repo_fixedcoin_v29.conf coin_core_repo.conf
./scripts/build_coin_core.sh
```

---

## Configuration Reference

### All Configuration Options

| Setting | Required | Description |
|---------|----------|-------------|
| `COIN_CORE_REPO_URL` | Yes | Git repository URL |
| `COIN_CORE_BRANCH` | No | Branch, tag, or commit to checkout |
| `COIN_CORE_BINARY_SUFFIX` | No | Suffix for output binary (e.g., `rincoin` → `stratum-proxy_rincoin`) |
| `COIN_CORE_CXX_STANDARD` | No | C++ standard (default: 20, older forks need 17) |
| `COIN_CORE_HASH_FUNCTION` | Yes | Method name on `CBlockHeader` for PoW hash |
| `COIN_CORE_LIB_DIR` | Yes | Path to .a library files (relative to coin_core/) |
| `COIN_CORE_LIB_PREFIX` | Yes | Library name prefix (e.g., `bitcoin`, `fixedcoin`) |
| `COIN_CORE_CONFIGURE_OPTS` | No | Configure/cmake options |
| `COIN_CORE_EXTRA_INCLUDES` | No | Extra include paths (relative to coin_core/src) |
| `COIN_CORE_EXTRA_LIBS` | No | Extra libraries to link |
| `COIN_CORE_EXTRA_LIB_DIRS` | No | Extra library search paths (relative to coin_core/) |

### Hash Function (`COIN_CORE_HASH_FUNCTION`)

| Value | Used By |
|-------|---------|
| `GetPoWHash` | Most altcoins: Litecoin, Rincoin, Dogecoin, etc. |
| `GetHash` | Bitcoin Core, Fixedcoin, and SHA256d forks |

### Library Location (`COIN_CORE_LIB_DIR`)

| Build System | Typical Location |
|--------------|------------------|
| Autotools | `src` |
| CMake | `build/lib` |

### Library Prefix (`COIN_CORE_LIB_PREFIX`)

The prefix for library names. For `libfixedcoin_consensus.a`, the prefix is `fixedcoin`.

| Coin | Prefix |
|------|--------|
| Bitcoin Core | `bitcoin` |
| Fixedcoin | `fixedcoin` |
| Litecoin | `litecoin` |
| Rincoin | `rincoin` |

---

## System Dependencies

```bash
# Base dependencies (all coins)
sudo apt-get install -y build-essential libtool autotools-dev automake \
    pkg-config bsdmainutils python3 libssl-dev libevent-dev libboost-all-dev

# For coins that don't bundle univalue
sudo apt-get install -y libunivalue-dev
```

---

## Adding a New Coin

### Step-by-Step

1. Copy a sample config that matches your coin's build system (Autotools or CMake)
2. Update `COIN_CORE_REPO_URL` and `COIN_CORE_BRANCH`
3. Run `./scripts/clone_coin_core.sh` and `./scripts/build_coin_core.sh`
4. Find library location and prefix (see FAQ below)
5. Set `COIN_CORE_LIB_DIR` and `COIN_CORE_LIB_PREFIX`
6. Determine hash function (see FAQ below)
7. Set `COIN_CORE_HASH_FUNCTION`
8. Build stratum-proxy: `cd build && cmake .. && make`
9. Fix any errors using FAQ below

### Template

```ini
COIN_CORE_REPO_URL=[repository URL]
COIN_CORE_BRANCH=[branch or tag]
COIN_CORE_BINARY_SUFFIX=[coin name for binary, e.g., rincoin]
COIN_CORE_HASH_FUNCTION=[GetPoWHash or GetHash]
COIN_CORE_LIB_DIR=[src or build/lib]
COIN_CORE_LIB_PREFIX=[bitcoin, litecoin, etc.]
COIN_CORE_CONFIGURE_OPTS=--disable-wallet --disable-tests --disable-bench
COIN_CORE_EXTRA_INCLUDES=
COIN_CORE_EXTRA_LIB_DIRS=
COIN_CORE_EXTRA_LIBS=
```

---

## FAQ / Troubleshooting

### How do I find where libraries are located?

After building the coin core, find the `.a` files:

```bash
find coin_core -name "*.a" | head -20
```

**Autotools builds** typically put libraries in:
- `coin_core/src/` (e.g., `librincoin_consensus.a`)
- `coin_core/src/.libs/`
- `coin_core/src/crypto/.libs/`

**CMake builds** typically put libraries in:
- `coin_core/build/lib/` (e.g., `libfixedcoin_consensus.a`)

Set `COIN_CORE_LIB_DIR` to the path relative to `coin_core/` (e.g., `src` or `build/lib`).

### How do I find the library prefix?

Look at the library filenames. For `libfixedcoin_consensus.a`, the prefix is `fixedcoin`.

```bash
ls coin_core/build/lib/*.a   # CMake
ls coin_core/src/*.a         # Autotools
```

### How do I find the hash function name?

Look at the coin's `primitives/block.h` file:

```bash
grep -n "GetPoWHash\|GetHash" coin_core/src/primitives/block.h
```

- If you see `GetPoWHash()` → use `GetPoWHash`
- If only `GetHash()` → use `GetHash`

### Build fails: "Consensus library not found"

1. Check if the library exists: `find coin_core -name "*consensus*"`
2. Update `COIN_CORE_LIB_DIR` to the correct path
3. Update `COIN_CORE_LIB_PREFIX` to match the library naming

### Build fails: "error: 'COIN_HASH_FUNCTION' is not a member"

The hash function name is wrong. Check `primitives/block.h` for the actual method name.

### Build fails: Missing header file

Add the missing include path to `COIN_CORE_EXTRA_INCLUDES`.

Example: If `mw/models/crypto/Hash.h` is missing and exists at `coin_core/src/libmw/include/mw/models/crypto/Hash.h`, add:
```
COIN_CORE_EXTRA_INCLUDES=libmw/include
```

### Build fails: Undefined reference to function

The coin requires additional libraries. Find them and add to `COIN_CORE_EXTRA_LIBS`:

```bash
# Find library containing the missing symbol
find coin_core -name "*.a" -exec nm {} \; 2>/dev/null | grep "T _ZN.*MissingFunction"
```

### How do I know if a coin uses Autotools or CMake?

- **Autotools**: Has `autogen.sh` or `configure.ac` in root
- **CMake**: Has `CMakeLists.txt` in root

```bash
ls coin_core/autogen.sh coin_core/CMakeLists.txt 2>/dev/null
```
