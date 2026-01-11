# README_AI.md - AI/LLM Integration Guide for stratum-proxy

## Document Purpose

This document is designed as a technical reference for AI models and developers to understand, maintain, and extend the stratum-proxy project. It contains:

1. **Architectural principles** that must not be violated
2. **Configuration system** and how to add new coins
3. **Real problems encountered** during development with their solutions
4. **Technical details** about stratum difficulty calculation

Priority: This document is written for AI consumption but remains human-readable.

---

## 1. Core Architectural Principle: Independence from Coin Core

### CRITICAL: The stratum-proxy code MUST NOT depend on specific coin implementations.

**Correct approach:**
- All coin-specific settings are in `coin_core_repo.conf`
- The proxy code uses generic interfaces (`CBlockHeader`, `uint256`)
- Hash function name is configured, not hardcoded
- Library paths and names are configured, not assumed

**Wrong approach:**
- Hardcoding `#include <rinhash.h>` or similar coin-specific headers
- Assuming library names like `libbitcoin_consensus.a`
- Assuming directory structure like `src/.libs/`
- Hardcoding hash function names like `GetPoWHash()`

### Implementation Pattern

```cpp
// CORRECT: Use macro from configuration
uint256 result = blockHeader.COIN_HASH_FUNCTION();

// WRONG: Hardcode function name
uint256 result = blockHeader.GetPoWHash();
```

The `COIN_HASH_FUNCTION` macro is defined via CMake from `coin_core_repo.conf`:
```cmake
add_compile_definitions(COIN_HASH_FUNCTION=${COIN_HASH_FUNCTION})
```

---

## 2. Configuration System (coin_core_repo.conf)

### All Supported Parameters

| Parameter | Required | Description | Example Values |
|-----------|----------|-------------|----------------|
| `COIN_CORE_REPO_URL` | Yes | Git repository URL | `https://github.com/bitcoin/bitcoin.git` |
| `COIN_CORE_BRANCH` | No | Branch/tag/commit | `v28.0`, `master`, `yenten-6` |
| `COIN_CORE_BINARY_SUFFIX` | Yes | Output binary suffix | `bitcoin`, `yenten`, `rincoin` |
| `COIN_CORE_CXX_STANDARD` | No | C++ standard (default: 20) | `17`, `20` |
| `COIN_CORE_HASH_FUNCTION` | Yes | Hash method name | `GetHash`, `GetPoWHash` |
| `COIN_CORE_LIB_DIR` | Yes | Library directory (relative) | `src/.libs`, `build/src` |
| `COIN_CORE_LIB_PREFIX` | Yes | Library name prefix | `bitcoin`, `litecoin` |
| `COIN_CORE_CONFIGURE_OPTS` | No | Build options | `--disable-wallet --disable-tests` |
| `COIN_CORE_EXTRA_INCLUDES` | No | Extra include paths | `libmw/include` |
| `COIN_CORE_EXTRA_LIB_DIRS` | No | Extra library directories | `src/crypto/argon2` |
| `COIN_CORE_EXTRA_LIBS` | No | Extra libraries to link | `argon2 secp256k1` |
| `COIN_CORE_SWAP_PREVHASH` | Yes | Swap prevhash bytes | `true`, `false` |
| `COIN_CORE_SWAP_MERKLE` | Yes | Swap merkle bytes | `true`, `false` |
| `COIN_CORE_DIFF_MULTIPLIER` | Yes | Difficulty multiplier | `1`, `0x100`, `0x10000` |
| `COIN_CORE_POW_LIMIT` | No | Consensus powLimit (documentation only) | `0x00000fffff...` |

---

## 3. Real Problems Encountered and Solutions

### 3.1. Hash Function Names Vary Between Coins

**Problem:** Different coins use different method names for PoW hash calculation.

| Coin Type | Hash Function | Notes |
|-----------|---------------|-------|
| Bitcoin/SHA256d | `GetHash()` | Returns SHA256d of header |
| Altcoins (Scrypt, YesPower, etc.) | `GetPoWHash()` | Returns algorithm-specific hash |

**Solution:** Configure via `COIN_CORE_HASH_FUNCTION` in conf file. CMake passes to compiler as `-DCOIN_HASH_FUNCTION=GetPoWHash`.

---

### 3.2. Library Names and Locations Vary

**Problem:** Coins use different build systems with different output locations.

| Build System | Library Location | Library Names |
|--------------|------------------|---------------|
| Autotools | `src/.libs/` | `libbitcoinconsensus.so`, `libbitcoin_crypto.a` |
| CMake (old) | `build/src/` | `libbitcoin_consensus.a` |
| CMake (v28+) | `build/src/` | `libbitcoin_consensus.a`, `libbitcoin_crypto_*.a` |

**Solution:** Configure `COIN_CORE_LIB_DIR` and `COIN_CORE_LIB_PREFIX` in conf file.

---

### 3.3. Extra Libraries for Specialized Algorithms

**Problem:** Some coins require additional libraries not present in standard Bitcoin.

| Coin | Extra Libraries | Purpose |
|------|-----------------|---------|
| Rincoin | `libargon2.a`, `libargon2_avx2.a`, `libargon2_avx512.a`, `libargon2_ssse3.a` | RinHash uses Argon2d |
| Rincoin | `libsecp256k1.a` (from secp256k1-zkp) | MWEB cryptography |
| Rincoin | `libbitcoin_util.a` | Utility functions |

**Solution:** Configure `COIN_CORE_EXTRA_LIBS` and `COIN_CORE_EXTRA_LIB_DIRS`:
```
COIN_CORE_EXTRA_LIB_DIRS=src/crypto/argon2 src/secp256k1-zkp/.libs src
COIN_CORE_EXTRA_LIBS=bitcoin_util argon2 argon2_avx2 argon2_avx512 argon2_ssse3 secp256k1
```

---

### 3.4. Extra Include Paths for Specialized Features

**Problem:** Some coins have headers in non-standard locations.

| Coin | Extra Include | Purpose |
|------|---------------|---------|
| Rincoin | `libmw/include` | MWEB (MimbleWimble) headers |

**Error without fix:**
```
fatal error: mw/models/crypto/Hash.h: No such file or directory
```

**Solution:** Configure `COIN_CORE_EXTRA_INCLUDES=libmw/include`

---

### 3.5. C++ Standard Compatibility

**Problem:** Older forks use deprecated C++ features removed in C++20.

| Issue | Affected Coins | Solution |
|-------|----------------|----------|
| `allocator<void>` removed in C++20 | Yenten, older Litecoin forks | Use `COIN_CORE_CXX_STANDARD=17` |
| `std::unary_function` removed | Very old forks | Use `COIN_CORE_CXX_STANDARD=17` |

---

### 3.6. Stratum Header Byte Ordering

**Problem:** Different algorithms expect different byte ordering for prevhash and merkle root from stratum protocol.

**Background:** Stratum protocol sends prevhash and merkle root as hex strings. The byte order depends on the PoW algorithm.

| Algorithm Family | SWAP_PREVHASH | SWAP_MERKLE | Notes |
|------------------|---------------|-------------|-------|
| SHA256d (Bitcoin) | `false` | `true` | Bitcoin standard |
| Scrypt (Litecoin) | `true` | `false` | Litecoin standard |
| YesPower, YesCrypt | `true` | `false` | Like Scrypt |
| Memory-hard (Argon2-based) | `true` | `false` | Assumed like Scrypt |

**Symptom when wrong:** Hash computation produces incorrect results, shares rejected with "H-not-zero" or similar errors.

**Solution:** Configure `COIN_CORE_SWAP_PREVHASH` and `COIN_CORE_SWAP_MERKLE` in conf file.

#### How to Determine Byte Ordering for a New Coin

1. **Check existing pool software (most reliable):**
   - Look in `yiimp/stratum/stratum.cpp` in the `g_algos[]` table
   - The `merkle_func` field indicates merkle handling:
     - `sha256_double_hash_hex` → `SWAP_MERKLE=true` (Bitcoin-style)
     - `sha256_merkle_hash` → `SWAP_MERKLE=false` (Scrypt-style)
   
2. **Check existing miners:**
   - Look at how miners like cpuminer-opt, SRBMiner, etc. construct block headers
   - Search for "prevhash" or "merkle" in their stratum handling code
   
3. **Check the coin's own stratum implementation:**
   - If the coin has its own stratum server (e.g., in `src/rpc/mining.cpp`), check how it serializes headers
   
4. **Use algorithm family as starting point:**
   - Bitcoin/SHA256d forks: `SWAP_PREVHASH=false`, `SWAP_MERKLE=true`
   - Litecoin/Scrypt forks: `SWAP_PREVHASH=true`, `SWAP_MERKLE=false`
   - Most altcoins with custom PoW: Follow Litecoin convention
   
5. **Empirical testing:**
   - If shares are rejected with hash errors, try toggling the swap flags
   - Enable debug logging to compare computed hash with expected values

---

### 3.7. CRITICAL: Difficulty Calculation (Most Complex Issue)

**Problem:** Initial implementation used `consensus.powLimit` from chainparams.cpp for difficulty calculation. This produced incorrect difficulty values that didn't match pool expectations.

**Root Cause Discovery:** Yiimp pools do NOT use `powLimit` for stratum difficulty. They use a completely different formula.

#### Yiimp Difficulty Formula (from yiimp/stratum/util.cpp)

```cpp
// Step 1: Extract hash_int from bytes [22-29] of the 32-byte hash
// This is get_hash_difficulty() in yiimp
uint64_t hash_int = extract_bytes_22_to_29_big_endian(hash);

// Step 2: Calculate base share difficulty
// YIIMP_DIFF1 is ALWAYS 0x0000ffff00000000, regardless of algorithm!
double share_diff = (double)0x0000ffff00000000ULL / (double)hash_int;

// Step 3: Apply algorithm-specific multiplier for comparison
// diff_multiplier comes from g_algos[] table in stratum.cpp
double effective_diff = share_diff * diff_multiplier;
```

#### Algorithm Difficulty Multipliers (from yiimp g_algos[])

| Algorithm | diff_multiplier | Hex Value |
|-----------|-----------------|-----------|
| sha256d, blake, decred, keccak | `1` | `0x1` |
| lyra2v2, lyra2v3, groestl, x11, x13, x16r, x16s | `256` | `0x100` |
| yespower, yescrypt, neoscrypt, argon2d | `65536` | `0x10000` |

#### Hash Int Extraction (Critical Detail)

The hash_int is extracted from bytes [22-29] of the hash in **big-endian order**:

```cpp
static uint64_t extractHashInt(const uint8_t hash[32]) {
    const uint8_t* p = hash;
    uint64_t v =
        (uint64_t)p[29] << 56 |
        (uint64_t)p[28] << 48 |
        (uint64_t)p[27] << 40 |
        (uint64_t)p[26] << 32 |
        (uint64_t)p[25] << 24 |
        (uint64_t)p[24] << 16 |
        (uint64_t)p[23] << 8  |
        (uint64_t)p[22] << 0;
    return v;
}
```

#### Why powLimit Is Wrong

`powLimit` defines the consensus minimum difficulty (difficulty 1 on chain). But stratum pools use a different "diff1" constant (`0x0000ffff00000000`) that has no relation to `powLimit`. Using `powLimit` produced difficulty values orders of magnitude off from what pools expected.

**Example with Yenten (YesPower):**
- powLimit-based calculation: difficulty ~0.00001 (wrong)
- yiimp formula: difficulty ~5.4 (correct)
- Pool difficulty: 0.1
- With correct formula: `5.4 >= 0.1` → share accepted ✓

---

## 4. Adding a New Coin Core

### Step-by-Step Checklist

1. **Research the coin:**
   - [ ] Identify PoW algorithm (SHA256d, Scrypt, YesPower, Argon2, etc.)
   - [ ] Find hash function name (usually `GetPoWHash()` for altcoins, `GetHash()` for Bitcoin)
   - [ ] Identify build system (autotools or CMake)
   - [ ] Check for special dependencies (MWEB, Argon2, etc.)

2. **Create configuration file:**
   - [ ] Copy existing sample from `coin_core_samples/` as template
   - [ ] Set `COIN_CORE_REPO_URL` and `COIN_CORE_BRANCH`
   - [ ] Set `COIN_CORE_HASH_FUNCTION`
   - [ ] Set `COIN_CORE_LIB_DIR` based on build system:
     - Autotools: `src/.libs`
     - CMake: `build/src` or similar
   - [ ] Set `COIN_CORE_LIB_PREFIX` (usually `bitcoin` or coin name)

3. **Determine byte ordering:**
   - [ ] SHA256d: `SWAP_PREVHASH=false`, `SWAP_MERKLE=true`
   - [ ] Scrypt/YesPower/Memory-hard: `SWAP_PREVHASH=true`, `SWAP_MERKLE=false`

4. **Determine difficulty multiplier:**
   - [ ] Check yiimp g_algos[] table for the algorithm
   - [ ] SHA256d: `1`
   - [ ] X11/X16R/Lyra2: `0x100`
   - [ ] YesPower/Argon2: `0x10000`
   - [ ] New algorithm: Start with `1`, test and adjust

5. **Check for extra dependencies:**
   - [ ] Build the coin core standalone first
   - [ ] Note any library errors and add to `COIN_CORE_EXTRA_LIBS`
   - [ ] Note any header errors and add to `COIN_CORE_EXTRA_INCLUDES`
   - [ ] Note any library path issues and add to `COIN_CORE_EXTRA_LIB_DIRS`

6. **Build and test:**
   ```bash
   cp coin_core_samples/your_config.conf coin_core_repo.conf
   make clone-core
   make build-core
   make build
   ./build/stratum-proxy-yourcoin stratum.toml
   ```

7. **Validate difficulty calculation:**
   - [ ] Run proxy with a test pool
   - [ ] Check that `meets_pool_difficulty` matches pool acceptance
   - [ ] If all shares show `meets_pool_difficulty:false` but pool accepts them, adjust `COIN_CORE_DIFF_MULTIPLIER`

---

## 5. Tested Coin Configurations

| Coin | Algorithm | Build System | Hash Function | Swap Prevhash | Swap Merkle | Diff Multiplier |
|------|-----------|--------------|---------------|---------------|-------------|-----------------|
| Bitcoin | SHA256d | CMake | `GetHash` | false | true | 1 |
| Yenten | YescryptR16 | Autotools | `GetPoWHash` | true | false | 0x10000 |
| Rincoin | RinHash | Autotools | `GetPoWHash` | true | false | 1 (TBD) |

Sample configurations in `coin_core_samples/`.

---

## 6. Common Build Errors and Solutions

### Error: `fatal error: primitives/block.h: No such file or directory`
**Cause:** Coin core not cloned or built.
**Solution:** Run `make clone-core && make build-core`

### Error: `fatal error: mw/models/crypto/Hash.h: No such file or directory`
**Cause:** MWEB headers not in include path.
**Solution:** Add `COIN_CORE_EXTRA_INCLUDES=libmw/include`

### Error: `undefined reference to 'argon2d_hash'`
**Cause:** Argon2 library not linked.
**Solution:** Add to `COIN_CORE_EXTRA_LIBS=argon2` and `COIN_CORE_EXTRA_LIB_DIRS=src/crypto/argon2`

### Error: `undefined reference to 'secp256k1_...'`
**Cause:** secp256k1 library not linked or wrong version.
**Solution:** Check if coin uses standard secp256k1 or secp256k1-zkp (MWEB coins), add appropriate path to `COIN_CORE_EXTRA_LIB_DIRS`

### Error: `std::allocator<void>` or similar C++20 errors
**Cause:** Older coin core incompatible with C++20.
**Solution:** Set `COIN_CORE_CXX_STANDARD=17`

### Error: `No rule to make target '...libsecp256k1.a'`
**Cause:** CMake cached a previous configuration with different library paths.
**Solution:** Clean build: `cd build && rm -rf * && cmake .. && make`

---

## 7. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        stratum-proxy                             │
├─────────────────────────────────────────────────────────────────┤
│  main.cpp                                                        │
│  ├── Stratum protocol handling                                   │
│  ├── Pool/miner connection management                            │
│  ├── Block header construction (with byte swap logic)            │
│  └── Share evaluation and forwarding                             │
├─────────────────────────────────────────────────────────────────┤
│  coin_hash_wrapper.cpp                                           │
│  ├── computeBlockHash() → calls COIN_HASH_FUNCTION              │
│  ├── shareDifficultyFromHash() → yiimp-compatible formula        │
│  ├── difficultyToTarget() → stores difficulty for comparison     │
│  └── checkHashMeetsTarget() → compares share_diff >= pool_diff   │
├─────────────────────────────────────────────────────────────────┤
│  coin_core/ (external, gitignored)                               │
│  ├── Built coin core libraries                                   │
│  ├── Provides: CBlockHeader, uint256, GetPoWHash()/GetHash()    │
│  └── Linked via CMake find_library()                             │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    coin_core_repo.conf                           │
│  ├── Repository URL and branch                                   │
│  ├── Hash function name                                          │
│  ├── Library paths and names                                     │
│  ├── Byte ordering flags                                         │
│  └── Difficulty multiplier                                       │
└─────────────────────────────────────────────────────────────────┘
```

---

## 8. Key Source Files

| File | Purpose | When to Modify |
|------|---------|----------------|
| `CMakeLists.txt` | Build configuration, reads conf file | Adding new conf parameters |
| `src/main.cpp` | Stratum protocol, header construction | Protocol changes only |
| `src/coin_hash_wrapper.cpp` | Hash computation, difficulty calc | Never (generic) |
| `coin_core_repo.conf` | Coin-specific settings | When adding new coin |
| `scripts/clone_coin_core.sh` | Clone coin repo | Rarely |
| `scripts/build_coin_core.sh` | Build coin core | Adding new build systems |

---

## 9. Testing Checklist

When adding a new coin, verify:

- [ ] `make clone-core` succeeds
- [ ] `make build-core` completes without errors
- [ ] `make build` produces `stratum-proxy-<suffix>` binary
- [ ] Proxy connects to pool and receives jobs
- [ ] Miner can connect to proxy
- [ ] Shares submitted by miner are forwarded to pool
- [ ] `share_eval` log shows reasonable difficulty values (not 0, not infinity)
- [ ] `meets_pool_difficulty` matches actual pool acceptance
- [ ] No "H-not-zero" or hash-related rejections from pool

---

## 10. Test Files Reference

The `/test` directory contains reference implementations and testing utilities for development and debugging. These are not part of the main build but serve as secondary help for understanding complex aspects of stratum-proxy integration.

| Test File | Purpose | When to Use |
|-----------|---------|-------------|
| `test_script.sh` | Automated build testing for multiple coin configurations | When adding new coins or regression testing builds |
| `test_yenten_hash.cpp` | Comprehensive Yenten hash computation with detailed header construction | Reference for understanding stratum header building and hash computation |
| `test_serialization.cpp` | Compares manual header construction vs CDataStream serialization | When debugging header serialization issues |
| `test_diff_calc.cpp` | Tests different difficulty calculation methods | When implementing difficulty calculation for new algorithms |
| `test_yespower_diff.cpp` | YesPower-specific difficulty calculations | Reference for memory-hard algorithm difficulty handling |

**Building tests:** Tests are standalone C++ files that link against the coin core. Build with:
```bash
g++ -std=c++17 -O2 -I../coin_core/src -I../coin_core/src/crypto \
    -o test_name test_name.cpp \
    ../coin_core/src/libbitcoinconsensus.a \
    [other libraries...] \
    -lcrypto -lpthread
```

---

## 11. Reference: Yiimp Source Code Locations

For difficulty calculation verification, refer to:

| File | Function | Purpose |
|------|----------|---------|
| `yiimp/stratum/util.cpp` | `target_to_diff()` | Converts target to difficulty |
| `yiimp/stratum/util.cpp` | `get_hash_difficulty()` | Extracts hash_int from hash bytes |
| `yiimp/stratum/stratum.cpp` | `g_algos[]` | Algorithm table with diff_multiplier |
| `yiimp/stratum/client_submit.cpp` | `client_submit()` | Share validation logic |

---

*Last updated: January 2026*
*Document version: 2.1*

