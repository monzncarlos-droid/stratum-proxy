// coin_hash_wrapper.cpp
// Implementation of the coin hash wrapper.
// This file links against the coin core's libraries and calls its hash function.
//
// The hash function is specified by COIN_CORE_HASH_FUNCTION in coin_core_repo.conf
// and passed to the compiler as -DCOIN_HASH_FUNCTION=GetPoWHash (or GetHash, etc.)

#include "coin_hash_wrapper.h"

#include <cstring>
#include <cmath>
#include <limits>

// Boost multiprecision for accurate 256-bit difficulty calculations
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

// Include coin core headers
// These are standard across Bitcoin/Litecoin forks
#include <primitives/block.h>
#include <uint256.h>
#include <serialize.h>
#include <streams.h>

// Ensure COIN_HASH_FUNCTION is defined (set via CMakeLists.txt from coin_core_repo.conf)
#ifndef COIN_HASH_FUNCTION
#error "COIN_HASH_FUNCTION must be defined (e.g., GetPoWHash or GetHash). Check coin_core_repo.conf"
#endif

namespace CoinHash {

// Helper: convert 32 bytes (little-endian) to cpp_int
static boost::multiprecision::cpp_int u256FromLeBytes(const uint8_t bytes32[32]) {
    using boost::multiprecision::cpp_int;
    cpp_int v = 0;
    for (int i = 31; i >= 0; --i) {
        v <<= 8;
        v |= static_cast<unsigned>(bytes32[i]);
    }
    return v;
}

void computeBlockHash(const uint8_t header[80], uint8_t hash[32]) {
    // Deserialize the 80-byte header into a CBlockHeader object
    CBlockHeader blockHeader;
    
    // Parse the header manually to avoid stream complexity
    // Layout: version(4) + prevhash(32) + merkleroot(32) + time(4) + bits(4) + nonce(4)
    std::memcpy(&blockHeader.nVersion, header + 0, 4);
    std::memcpy(blockHeader.hashPrevBlock.begin(), header + 4, 32);
    std::memcpy(blockHeader.hashMerkleRoot.begin(), header + 36, 32);
    std::memcpy(&blockHeader.nTime, header + 68, 4);
    std::memcpy(&blockHeader.nBits, header + 72, 4);
    std::memcpy(&blockHeader.nNonce, header + 76, 4);
    
    // Call the coin core's hash function (configured in coin_core_repo.conf)
    // COIN_HASH_FUNCTION is set to GetPoWHash, GetHash, etc. via -D compiler flag
    uint256 result = blockHeader.COIN_HASH_FUNCTION();
    
    // Copy result to output (uint256 stores as little-endian)
    std::memcpy(hash, result.begin(), 32);
}

// Forward declaration
static uint64_t extractHashInt(const uint8_t hash[32]);

bool checkHashMeetsTarget(const uint8_t hash[32], const uint32_t target[8]) {
    // Simple approach: compare share difficulty against pool difficulty
    // The target array stores the pool difficulty as a double in target[0-1]
    
    double pool_difficulty;
    std::memcpy(&pool_difficulty, target, sizeof(double));
    
    double share_difficulty = shareDifficultyFromHash(hash);
    
    return share_difficulty >= pool_difficulty;
}

// Helper: Parse powLimit hex string to cpp_int
// Handles both "0x3fff..." and "0000ffff..." formats
static boost::multiprecision::cpp_int parsePowLimitHex(const char* hexStr) {
    using boost::multiprecision::cpp_int;
    std::string s(hexStr);
    
    // Remove 0x prefix if present
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s = s.substr(2);
    }
    
    // Parse hex string to cpp_int
    cpp_int value = 0;
    for (char c : s) {
        value <<= 4;
        if (c >= '0' && c <= '9') {
            value |= (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            value |= (c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            value |= (c - 'A' + 10);
        }
    }
    return value;
}

// =============================================================================
// Yiimp-compatible stratum difficulty calculation
// =============================================================================
//
// YIIMP APPROACH (from util.cpp and client_submit.cpp):
//
// 1. get_hash_difficulty() extracts bytes [22-29] of the 32-byte hash as uint64_t
//    This is the "hash_int" - the middle 8 bytes of the hash (big-endian extraction)
//
// 2. share_diff = 0x0000ffff00000000 / hash_int
//    This is a CONSTANT diff1, not algorithm-dependent!
//
// 3. The diff_multiplier from g_algos is ONLY used for target comparison:
//    user_target = share_to_target(difficulty) * diff_multiplier
//    hash_int_scaled = hash_int / 0x10000
//    Accept if: hash_int_scaled <= user_target
//
// So for share difficulty display: always use 0x0000ffff00000000 / hash_int
// For target comparison: apply diff_multiplier to the target, not the diff1
//
// =============================================================================

// Extract hash difficulty like yiimp's get_hash_difficulty()
// Returns bytes [22-29] of the hash as a big-endian uint64_t
static uint64_t extractHashInt(const uint8_t hash[32]) {
    // Hash is in little-endian format (Bitcoin convention)
    // yiimp reads bytes p[22] through p[29] in big-endian order
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

// Yiimp diff1 constant for share difficulty calculation
// This is ALWAYS 0x0000ffff00000000, independent of algorithm
static constexpr uint64_t YIIMP_DIFF1 = 0x0000ffff00000000ULL;

// COIN_DIFF_MULTIPLIER from coin_core_repo.conf (defaults to 1)
// This is used for target comparison, not share difficulty calculation
#ifndef COIN_DIFF_MULTIPLIER
#define COIN_DIFF_MULTIPLIER 1
#endif

void difficultyToTarget(double difficulty, uint32_t target[8]) {
    // We don't actually need a proper target for our purposes.
    // The checkHashMeetsTarget function will directly compare share_diff >= pool_diff.
    // This function is kept for API compatibility but we store the difficulty directly.
    
    // Store the difficulty as a double in the target array for later retrieval
    // We use target[0-1] to store the double (8 bytes)
    std::memset(target, 0, 32);
    std::memcpy(target, &difficulty, sizeof(double));
}

double shareDifficultyFromHash(const uint8_t hash[32]) {
    // Yiimp share_diff calculation from util.cpp target_to_diff():
    //   share_diff = 0x0000ffff00000000 / hash_int
    //
    // where hash_int is extracted from bytes [22-29] of the hash
    //
    // IMPORTANT: This gives the "base" difficulty. For algorithms with
    // diff_multiplier (like YesPower with 0x10000), the EFFECTIVE difficulty
    // that the pool compares against is:
    //   effective_diff = share_diff * diff_multiplier
    //
    // This is because yiimp's target comparison uses:
    //   user_target = share_to_target(difficulty) * diff_multiplier
    // And the share_diff is calculated without the multiplier.
    
    uint64_t hash_int = extractHashInt(hash);
    if (hash_int == 0) {
        return std::numeric_limits<double>::infinity();
    }

    double base_diff = (double)YIIMP_DIFF1 / (double)hash_int;
    
    // Apply diff_multiplier to get the effective pool-comparable difficulty
    return base_diff * (double)COIN_DIFF_MULTIPLIER;
}

} // namespace CoinHash
