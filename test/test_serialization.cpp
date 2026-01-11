// Test to compare manual header construction vs CDataStream serialization
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <primitives/block.h>
#include <streams.h>
#include <version.h>
#include <openssl/sha.h>

static void hexstrToBytes(const std::string &hex, uint8_t *out) {
    for (size_t i = 0; i < hex.size(); i += 2) {
        out[i / 2] = (uint8_t)std::stoul(hex.substr(i, 2), nullptr, 16);
    }
}

static std::string hexStr(const uint8_t *data, size_t len) {
    static const char hex[] = "0123456789abcdef";
    std::string s;
    s.reserve(len * 2);
    for (size_t i = 0; i < len; i++) {
        s.push_back(hex[data[i] >> 4]);
        s.push_back(hex[data[i] & 0x0f]);
    }
    return s;
}

static inline void le32enc(void *dst, uint32_t x) {
    uint8_t *p = (uint8_t *)dst;
    p[0] = (uint8_t)(x);
    p[1] = (uint8_t)(x >> 8);
    p[2] = (uint8_t)(x >> 16);
    p[3] = (uint8_t)(x >> 24);
}

int main() {
    // Data from the log
    const char* extranonce1_hex = "4fffffa6";
    const char* extranonce2_hex = "51006064";
    const char* nonce_hex = "80000013";
    
    // From mining.notify
    const char* prevhash_hex = "6d920eda2f85788ef91c1dec08f4b7e4bab7f27b1faf5e72cb2d9b50120764fa";
    const char* coinb1_hex = "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff2203ea23200491ce626908";
    const char* coinb2_hex = "0f2f3134312e382e3139392e3137372f00000000040000000000000000266a24aa21a9ede2f61c3f71d1defd3fa999dfa36953755c690689799962b48bebd836974e8cf920674f42000000001976a914e24695d498b5756e26f92c5d7f313fc0632440ab88ac40597307000000001976a914a93ca5dd3396df730a34a69621d5267aa976f92788ac20bcbe00000000001976a914e532b102f26f2bc315785c26bc06136034ffe79188ac00000000";
    const char* version_hex = "20000000";
    const char* nbits_hex = "1e00cc44";
    const char* ntime_hex = "6962ce91";
    
    printf("=== Testing CBlockHeader serialization ===\n\n");
    
    // Parse values
    uint32_t version = (uint32_t)std::stoul(version_hex, nullptr, 16);
    uint32_t nbits = (uint32_t)std::stoul(nbits_hex, nullptr, 16);
    uint32_t ntime = (uint32_t)std::stoul(ntime_hex, nullptr, 16);
    uint32_t nonce = (uint32_t)std::stoul(nonce_hex, nullptr, 16);
    
    printf("Parsed values:\n");
    printf("  version = 0x%08x\n", version);
    printf("  nbits   = 0x%08x\n", nbits);
    printf("  ntime   = 0x%08x\n", ntime);
    printf("  nonce   = 0x%08x\n\n", nonce);
    
    // Build coinbase and merkle root
    std::string coinbase_hex = std::string(coinb1_hex) + extranonce1_hex + extranonce2_hex + coinb2_hex;
    std::vector<uint8_t> coinbase(coinbase_hex.size() / 2);
    hexstrToBytes(coinbase_hex, coinbase.data());
    
    // SHA256d for merkle root (no branches in this case)
    uint8_t hash1[32], hash2[32];
    SHA256(coinbase.data(), coinbase.size(), hash1);
    SHA256(hash1, 32, hash2);
    printf("Merkle root (raw bytes): %s\n\n", hexStr(hash2, 32).c_str());
    
    uint8_t prevhash[32];
    hexstrToBytes(prevhash_hex, prevhash);
    
    // ========================================
    // Method 1: Construct CBlockHeader directly and serialize
    // ========================================
    printf("=== Method 1: CBlockHeader direct construction ===\n");
    {
        CBlockHeader blk;
        blk.nVersion = version;  // int32_t, stored as-is
        
        // prevhash from pool is a hex string - just copy the bytes
        memcpy(blk.hashPrevBlock.begin(), prevhash, 32);
        
        // merkle root from SHA256d
        memcpy(blk.hashMerkleRoot.begin(), hash2, 32);
        
        blk.nTime = ntime;
        blk.nBits = nbits;
        blk.nNonce = nonce;
        
        // Serialize to see what bytes go to yespower
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << blk;
        
        printf("Serialized header (%zu bytes):\n", ss.size());
        printf("  %s\n", hexStr((uint8_t*)&ss[0], ss.size()).c_str());
        
        // Call GetPoWHash
        uint256 hash = blk.GetPoWHash();
        printf("GetPoWHash result: %s\n", hash.GetHex().c_str());
        printf("GetPoWHash raw:    %s\n\n", hexStr(hash.begin(), 32).c_str());
    }
    
    // ========================================
    // Method 2: Try with prevhash byte-swapped (4-byte words)
    // ========================================
    printf("=== Method 2: prevhash with 4-byte word swap ===\n");
    {
        CBlockHeader blk;
        blk.nVersion = version;
        
        // Swap each 4-byte word of prevhash
        for (int i = 0; i < 8; i++) {
            uint32_t word;
            memcpy(&word, prevhash + i*4, 4);
            uint32_t swapped = __builtin_bswap32(word);
            memcpy(blk.hashPrevBlock.begin() + i*4, &swapped, 4);
        }
        
        memcpy(blk.hashMerkleRoot.begin(), hash2, 32);
        blk.nTime = ntime;
        blk.nBits = nbits;
        blk.nNonce = nonce;
        
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << blk;
        
        printf("Serialized header (%zu bytes):\n", ss.size());
        printf("  %s\n", hexStr((uint8_t*)&ss[0], ss.size()).c_str());
        
        uint256 hash = blk.GetPoWHash();
        printf("GetPoWHash result: %s\n", hash.GetHex().c_str());
        printf("GetPoWHash raw:    %s\n\n", hexStr(hash.begin(), 32).c_str());
    }
    
    // ========================================
    // Method 3: Both prevhash and merkle swapped
    // ========================================
    printf("=== Method 3: Both prevhash and merkle swapped ===\n");
    {
        CBlockHeader blk;
        blk.nVersion = version;
        
        for (int i = 0; i < 8; i++) {
            uint32_t word;
            memcpy(&word, prevhash + i*4, 4);
            uint32_t swapped = __builtin_bswap32(word);
            memcpy(blk.hashPrevBlock.begin() + i*4, &swapped, 4);
        }
        
        for (int i = 0; i < 8; i++) {
            uint32_t word;
            memcpy(&word, hash2 + i*4, 4);
            uint32_t swapped = __builtin_bswap32(word);
            memcpy(blk.hashMerkleRoot.begin() + i*4, &swapped, 4);
        }
        
        blk.nTime = ntime;
        blk.nBits = nbits;
        blk.nNonce = nonce;
        
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << blk;
        
        printf("Serialized header (%zu bytes):\n", ss.size());
        printf("  %s\n", hexStr((uint8_t*)&ss[0], ss.size()).c_str());
        
        uint256 hash = blk.GetPoWHash();
        printf("GetPoWHash result: %s\n", hash.GetHex().c_str());
        printf("GetPoWHash raw:    %s\n\n", hexStr(hash.begin(), 32).c_str());
    }
    
    // ========================================
    // Method 4: Only merkle swapped
    // ========================================
    printf("=== Method 4: Only merkle swapped ===\n");
    {
        CBlockHeader blk;
        blk.nVersion = version;
        
        memcpy(blk.hashPrevBlock.begin(), prevhash, 32);
        
        for (int i = 0; i < 8; i++) {
            uint32_t word;
            memcpy(&word, hash2 + i*4, 4);
            uint32_t swapped = __builtin_bswap32(word);
            memcpy(blk.hashMerkleRoot.begin() + i*4, &swapped, 4);
        }
        
        blk.nTime = ntime;
        blk.nBits = nbits;
        blk.nNonce = nonce;
        
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << blk;
        
        printf("Serialized header (%zu bytes):\n", ss.size());
        printf("  %s\n", hexStr((uint8_t*)&ss[0], ss.size()).c_str());
        
        uint256 hash = blk.GetPoWHash();
        printf("GetPoWHash result: %s\n", hash.GetHex().c_str());
        printf("GetPoWHash raw:    %s\n\n", hexStr(hash.begin(), 32).c_str());
    }
    
    return 0;
}
