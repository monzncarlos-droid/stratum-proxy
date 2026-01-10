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

bool checkHashMeetsTarget(const uint8_t hash[32], const uint32_t target[8]) {
    // Compare hash against target (both little-endian 256-bit integers)
    // hash <= target means the hash is valid
    for (int i = 7; i >= 0; --i) {
        uint32_t h = 0;
        std::memcpy(&h, hash + i * 4, 4);
        if (h < target[i]) return true;
        if (h > target[i]) return false;
    }
    return true; // Equal
}

void difficultyToTarget(double difficulty, uint32_t target[8]) {
    std::memset(target, 0, 32);
    if (difficulty == 0.0) {
        std::memset(target, 0xff, 32);
        return;
    }

    int k;
    double diff = difficulty;
    for (k = 6; k > 0 && diff > 1.0; k--) {
        diff /= 4294967296.0;
    }

    uint64_t m = static_cast<uint64_t>(4294901760.0 / diff);
    if (m == 0 && k == 6) {
        std::memset(target, 0xff, 32);
    } else {
        target[k] = static_cast<uint32_t>(m);
        target[k + 1] = static_cast<uint32_t>(m >> 32);
    }
}

double shareDifficultyFromHash(const uint8_t hash[32]) {
    using boost::multiprecision::cpp_dec_float_50;
    using boost::multiprecision::cpp_int;

    // diff1 target: 0xffff0000 at word index 6 (little-endian word order)
    // This matches the standard Bitcoin/Litecoin stratum difficulty convention
    cpp_int diff1 = cpp_int(0xffff0000);
    diff1 <<= (32 * 6);

    cpp_int h = u256FromLeBytes(hash);
    if (h == 0) {
        return std::numeric_limits<double>::infinity();
    }

    cpp_dec_float_50 d = cpp_dec_float_50(diff1) / cpp_dec_float_50(h);
    return d.convert_to<double>();
}

} // namespace CoinHash
