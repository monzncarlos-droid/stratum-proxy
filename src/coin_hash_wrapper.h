// coin_hash_wrapper.h
// Provides a unified interface to call the coin core's block header hash function.
// This wrapper adapts to the specific coin core that was built.

#pragma once

#include <cstdint>
#include <array>
#include <vector>

namespace CoinHash {

// Compute the PoW hash of a block header.
// Input: 80-byte block header (as per Bitcoin protocol)
// Output: 32-byte hash (little-endian, as per Bitcoin protocol)
//
// This function calls the coin core's CBlockHeader::GetPoWHash() or GetHash()
// internally, adapting to the specific coin's hashing algorithm.
void computeBlockHash(const uint8_t header[80], uint8_t hash[32]);

// Check if a hash meets a target difficulty.
// Returns true if hash <= target (both interpreted as little-endian 256-bit integers).
bool checkHashMeetsTarget(const uint8_t hash[32], const uint32_t target[8]);

// Convert difficulty to target words (same as tnn-miner convention).
void difficultyToTarget(double difficulty, uint32_t target[8]);

// Compute share difficulty from a hash.
double shareDifficultyFromHash(const uint8_t hash[32]);

} // namespace CoinHash
