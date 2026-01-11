// test_yenten_hash.cpp - Test Yenten (YesPower) hash computation
// Build with:
// g++ -std=c++17 -O2 -I../coin_core/src -I../coin_core/src/crypto \
//     -o test_yenten_hash test_yenten_hash.cpp \
//     ../coin_core/src/libbitcoinconsensus.a \
//     ../coin_core/src/.libs/libbitcoin_crypto.a \
//     ../coin_core/src/secp256k1/.libs/libsecp256k1.a \
//     -lcrypto -lpthread

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <array>
#include <iomanip>
#include <sstream>

#include <openssl/sha.h>
#include <primitives/block.h>
#include <streams.h>
#include <version.h>

static inline void le32enc(void *dst, uint32_t x) {
    uint8_t *p = (uint8_t *)dst;
    p[0] = (uint8_t)(x);
    p[1] = (uint8_t)(x >> 8);
    p[2] = (uint8_t)(x >> 16);
    p[3] = (uint8_t)(x >> 24);
}

static inline void be32enc(void *dst, uint32_t x) {
    uint8_t *p = (uint8_t *)dst;
    p[3] = (uint8_t)(x);
    p[2] = (uint8_t)(x >> 8);
    p[1] = (uint8_t)(x >> 16);
    p[0] = (uint8_t)(x >> 24);
}

static void hexstrToBytes(const std::string &hex, uint8_t *out) {
    for (size_t i = 0; i < hex.size(); i += 2) {
        out[i / 2] = (uint8_t)std::stoul(hex.substr(i, 2), nullptr, 16);
    }
}

static std::string hexStr(const uint8_t *data, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    return oss.str();
}

static std::vector<uint8_t> sha256d(const std::vector<uint8_t> &data) {
    uint8_t hash1[32], hash2[32];
    SHA256(data.data(), data.size(), hash1);
    SHA256(hash1, 32, hash2);
    return std::vector<uint8_t>(hash2, hash2 + 32);
}

static std::vector<uint8_t> calculate_merkle_root(const std::vector<uint8_t> &coinbase,
                                                   const std::vector<std::string> &branches) {
    std::vector<uint8_t> root = sha256d(coinbase);
    for (const auto &branch_hex : branches) {
        std::vector<uint8_t> branch(32);
        hexstrToBytes(branch_hex, branch.data());
        std::vector<uint8_t> concat(64);
        std::memcpy(concat.data(), root.data(), 32);
        std::memcpy(concat.data() + 32, branch.data(), 32);
        root = sha256d(concat);
    }
    return root;
}

static double share_difficulty_from_hash(const uint8_t hash[32]) {
    double diff1 = ldexp((double)0xffff0000, 192);
    double h = 0;
    for (int i = 31; i >= 0; --i) {
        h = h * 256.0 + hash[i];
    }
    if (h == 0) return 1e30;
    return diff1 / h;
}

int main() {
    // Data from the log
    const char* extranonce1_hex = "17fffd50";
    const char* extranonce2_hex = "3b633980";
    const char* ntime_hex = "6962be6a";
    const char* nonce_hex = "45555932";
    
    const char* prevhash_hex = "dda42af4e6306c0dd3cdaee1d9302f78f2ea235dd38ab24386854fd0b6736e7a";
    const char* coinb1_hex = "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff2103c92320046bbe626908";
    const char* coinb2_hex = "0e2f61696b61706f6f6c2e636f6d2f00000000020000000000000000266a24aa21a9ede1bfc80c0e1f4dd514a1bbd62eec03c2da5164af63e6843339ab777e95c5c9318f6d824a000000001976a91453d69b22eaf6c06a399f00044ea7609684278d3688ac00000000";
    const char* version_hex = "20000000";
    const char* nbits_hex = "1d53e95d";
    std::vector<std::string> merkle_branches = {"df7268130994b7aae2480972f11eb7d06addda97b4891b5cea89c7d3321d66aa"};
    
    double pool_difficulty = 1.0;
    
    printf("=== Testing Yenten (YesPower) hash computation ===\n\n");
    
    // Parse values
    uint32_t version = (uint32_t)std::stoul(version_hex, nullptr, 16);
    uint32_t nbits = (uint32_t)std::stoul(nbits_hex, nullptr, 16);
    uint32_t ntime = (uint32_t)std::stoul(ntime_hex, nullptr, 16);
    uint32_t nonce = (uint32_t)std::stoul(nonce_hex, nullptr, 16);
    
    // Build coinbase and merkle root
    std::string coinbase_hex = std::string(coinb1_hex) + extranonce1_hex + extranonce2_hex + coinb2_hex;
    std::vector<uint8_t> coinbase(coinbase_hex.size() / 2);
    hexstrToBytes(coinbase_hex, coinbase.data());
    std::vector<uint8_t> merkle_root = calculate_merkle_root(coinbase, merkle_branches);
    
    uint8_t prevhash[32];
    hexstrToBytes(prevhash_hex, prevhash);
    
    printf("Pool prevhash:  %s\n", prevhash_hex);
    printf("Merkle root:    %s\n", hexStr(merkle_root.data(), 32).c_str());
    printf("Version:        0x%08x\n", version);
    printf("nBits:          0x%08x\n", nbits);
    printf("nTime:          0x%08x\n", ntime);
    printf("Nonce:          0x%08x\n\n", nonce);
    
    // ========================================
    // Method 1: Current stratum-proxy approach (Rincoin style)
    // swap_prev_hash=true, swap_merkle_root=false
    // ========================================
    printf("=== Method 1: Rincoin style (swap_prev_hash=true, swap_merkle=false) ===\n");
    {
        std::array<uint8_t, 80> hdr{};
        
        le32enc(hdr.data() + 0, version);
        
        // Swap each 4-byte word of prevhash using be32enc
        for (int i = 0; i < 8; i++) {
            uint32_t word;
            memcpy(&word, prevhash + i*4, 4);
            be32enc(hdr.data() + 4 + i*4, word);
        }
        
        // Merkle root as-is
        memcpy(hdr.data() + 36, merkle_root.data(), 32);
        
        le32enc(hdr.data() + 68, ntime);
        le32enc(hdr.data() + 72, nbits);
        le32enc(hdr.data() + 76, nonce);
        
        printf("Header hex:     %s\n", hexStr(hdr.data(), 80).c_str());
        
        // Use Yenten core's GetPoWHash
        CBlockHeader blk;
        memcpy(&blk.nVersion, hdr.data(), 4);
        memcpy(blk.hashPrevBlock.begin(), hdr.data() + 4, 32);
        memcpy(blk.hashMerkleRoot.begin(), hdr.data() + 36, 32);
        memcpy(&blk.nTime, hdr.data() + 68, 4);
        memcpy(&blk.nBits, hdr.data() + 72, 4);
        memcpy(&blk.nNonce, hdr.data() + 76, 4);
        
        uint256 hash = blk.GetPoWHash();
        double diff = share_difficulty_from_hash(hash.begin());
        bool meets = diff >= pool_difficulty;
        
        printf("Hash:           %s\n", hash.GetHex().c_str());
        printf("Difficulty:     %.12e\n", diff);
        printf("Meets %.4f?  %s\n\n", pool_difficulty, meets ? "YES ✓" : "NO ✗");
    }
    
    // ========================================
    // Method 2: YesPower style from tnn-miner
    // swap_prev_hash=false, swap_merkle_root=true, ENDIAN_SWAP_32
    // ========================================
    printf("=== Method 2: YesPower style (swap_prev_hash=false, swap_merkle=true) ===\n");
    {
        std::array<uint8_t, 80> hdr{};
        
        le32enc(hdr.data() + 0, version);
        
        // Prevhash - NO swapping
        memcpy(hdr.data() + 4, prevhash, 32);
        
        // Merkle root - use be32enc (like tnn-miner does)
        for (int i = 0; i < 8; i++) {
            uint32_t word;
            memcpy(&word, merkle_root.data() + i*4, 4);
            be32enc(hdr.data() + 36 + i*4, word);
        }
        
        le32enc(hdr.data() + 68, ntime);
        le32enc(hdr.data() + 72, nbits);
        le32enc(hdr.data() + 76, nonce);
        
        printf("Header hex:     %s\n", hexStr(hdr.data(), 80).c_str());
        
        CBlockHeader blk;
        memcpy(&blk.nVersion, hdr.data(), 4);
        memcpy(blk.hashPrevBlock.begin(), hdr.data() + 4, 32);
        memcpy(blk.hashMerkleRoot.begin(), hdr.data() + 36, 32);
        memcpy(&blk.nTime, hdr.data() + 68, 4);
        memcpy(&blk.nBits, hdr.data() + 72, 4);
        memcpy(&blk.nNonce, hdr.data() + 76, 4);
        
        uint256 hash = blk.GetPoWHash();
        double diff = share_difficulty_from_hash(hash.begin());
        bool meets = diff >= pool_difficulty;
        
        printf("Hash:           %s\n", hash.GetHex().c_str());
        printf("Difficulty:     %.12e\n", diff);
        printf("Meets %.4f?  %s\n\n", pool_difficulty, meets ? "YES ✓" : "NO ✗");
    }
    
    // ========================================
    // Method 3: Both swapped
    // ========================================
    printf("=== Method 3: Both swapped (swap_prev_hash=true, swap_merkle=true) ===\n");
    {
        std::array<uint8_t, 80> hdr{};
        
        le32enc(hdr.data() + 0, version);
        
        // Prevhash - swap each 4-byte word using be32enc
        for (int i = 0; i < 8; i++) {
            uint32_t word;
            memcpy(&word, prevhash + i*4, 4);
            be32enc(hdr.data() + 4 + i*4, word);
        }
        
        // Merkle root - also swap each 4-byte word using be32enc
        for (int i = 0; i < 8; i++) {
            uint32_t word;
            memcpy(&word, merkle_root.data() + i*4, 4);
            be32enc(hdr.data() + 36 + i*4, word);
        }
        
        le32enc(hdr.data() + 68, ntime);
        le32enc(hdr.data() + 72, nbits);
        le32enc(hdr.data() + 76, nonce);
        
        printf("Header hex:     %s\n", hexStr(hdr.data(), 80).c_str());
        
        CBlockHeader blk;
        memcpy(&blk.nVersion, hdr.data(), 4);
        memcpy(blk.hashPrevBlock.begin(), hdr.data() + 4, 32);
        memcpy(blk.hashMerkleRoot.begin(), hdr.data() + 36, 32);
        memcpy(&blk.nTime, hdr.data() + 68, 4);
        memcpy(&blk.nBits, hdr.data() + 72, 4);
        memcpy(&blk.nNonce, hdr.data() + 76, 4);
        
        uint256 hash = blk.GetPoWHash();
        double diff = share_difficulty_from_hash(hash.begin());
        bool meets = diff >= pool_difficulty;
        
        printf("Hash:           %s\n", hash.GetHex().c_str());
        printf("Difficulty:     %.12e\n", diff);
        printf("Meets %.4f?  %s\n\n", pool_difficulty, meets ? "YES ✓" : "NO ✗");
    }
    
    // ========================================
    // Method 4: Neither swapped (raw copy)
    // ========================================
    printf("=== Method 4: No swapping (swap_prev_hash=false, swap_merkle=false) ===\n");
    {
        std::array<uint8_t, 80> hdr{};
        
        le32enc(hdr.data() + 0, version);
        
        // Prevhash - direct copy
        memcpy(hdr.data() + 4, prevhash, 32);
        
        // Merkle root - direct copy
        memcpy(hdr.data() + 36, merkle_root.data(), 32);
        
        le32enc(hdr.data() + 68, ntime);
        le32enc(hdr.data() + 72, nbits);
        le32enc(hdr.data() + 76, nonce);
        
        printf("Header hex:     %s\n", hexStr(hdr.data(), 80).c_str());
        
        CBlockHeader blk;
        memcpy(&blk.nVersion, hdr.data(), 4);
        memcpy(blk.hashPrevBlock.begin(), hdr.data() + 4, 32);
        memcpy(blk.hashMerkleRoot.begin(), hdr.data() + 36, 32);
        memcpy(&blk.nTime, hdr.data() + 68, 4);
        memcpy(&blk.nBits, hdr.data() + 72, 4);
        memcpy(&blk.nNonce, hdr.data() + 76, 4);
        
        uint256 hash = blk.GetPoWHash();
        double diff = share_difficulty_from_hash(hash.begin());
        bool meets = diff >= pool_difficulty;
        
        printf("Hash:           %s\n", hash.GetHex().c_str());
        printf("Difficulty:     %.12e\n", diff);
        printf("Meets %.4f?  %s\n\n", pool_difficulty, meets ? "YES ✓" : "NO ✗");
    }
    
    printf("stratum-proxy reported hash: 5cc75c21b08efe68c1e140b74ff2ab1e378d50ac6e53558c0a284db5f7c485b3\n");
    printf("stratum-proxy reported diff: 3.32012550424e-10\n");
    
    // ========================================
    // Method 5: Rincoin style but with nonce parsed differently
    // ========================================
    printf("\n=== Method 5: Rincoin style, nonce as LE bytes ===\n");
    {
        std::array<uint8_t, 80> hdr{};
        
        le32enc(hdr.data() + 0, version);
        
        // Prevhash - swap each 4-byte word
        for (int i = 0; i < 8; i++) {
            uint32_t word;
            memcpy(&word, prevhash + i*4, 4);
            uint32_t swapped = __builtin_bswap32(word);
            le32enc(hdr.data() + 4 + i*4, swapped);
        }
        
        // Merkle root as-is
        memcpy(hdr.data() + 36, merkle_root.data(), 32);
        
        le32enc(hdr.data() + 68, ntime);
        le32enc(hdr.data() + 72, nbits);
        
        // Nonce - parse as LE bytes instead of value
        uint8_t nonce_bytes[4];
        hexstrToBytes(nonce_hex, nonce_bytes);
        uint32_t nonce_le = nonce_bytes[0] | (nonce_bytes[1] << 8) | (nonce_bytes[2] << 16) | (nonce_bytes[3] << 24);
        le32enc(hdr.data() + 76, nonce_le);
        
        printf("Nonce value:    0x%08x (original) -> 0x%08x (LE parsed)\n", nonce, nonce_le);
        printf("Header hex:     %s\n", hexStr(hdr.data(), 80).c_str());
        
        CBlockHeader blk;
        memcpy(&blk.nVersion, hdr.data(), 4);
        memcpy(blk.hashPrevBlock.begin(), hdr.data() + 4, 32);
        memcpy(blk.hashMerkleRoot.begin(), hdr.data() + 36, 32);
        memcpy(&blk.nTime, hdr.data() + 68, 4);
        memcpy(&blk.nBits, hdr.data() + 72, 4);
        memcpy(&blk.nNonce, hdr.data() + 76, 4);
        
        uint256 hash = blk.GetPoWHash();
        double diff = share_difficulty_from_hash(hash.begin());
        bool meets = diff >= pool_difficulty;
        
        printf("Hash:           %s\n", hash.GetHex().c_str());
        printf("Difficulty:     %.12e\n", diff);
        printf("Meets %.4f?  %s\n\n", pool_difficulty, meets ? "YES ✓" : "NO ✗");
    }
    
    return 0;
}

// After main(), add more analysis
void analyze_hash(const uint8_t hash[32]) {
    printf("Hash bytes (LE): ");
    for (int i = 0; i < 32; i++) printf("%02x", hash[i]);
    printf("\n");
    
    // Count leading zero bits (from MSB end, i.e., from byte 31)
    int leading_zeros = 0;
    for (int i = 31; i >= 0; i--) {
        if (hash[i] == 0) {
            leading_zeros += 8;
        } else {
            for (int b = 7; b >= 0; b--) {
                if (hash[i] & (1 << b)) break;
                leading_zeros++;
            }
            break;
        }
    }
    printf("Leading zero bits: %d\n", leading_zeros);
}
