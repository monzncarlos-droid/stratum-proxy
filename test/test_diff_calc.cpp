// Test difficulty calculation
#include <cstdio>
#include <cstdint>
#include <cmath>

// Standard Bitcoin difficulty calculation
static double share_difficulty_from_hash(const uint8_t hash[32]) {
    // diff1 = 0xffff0000 * 2^192
    double diff1 = ldexp((double)0xffff0000, 192);
    double h = 0;
    for (int i = 31; i >= 0; --i) {
        h = h * 256.0 + hash[i];
    }
    if (h == 0) return 1e30;
    return diff1 / h;
}

// Alternative: difficulty based on leading zeros
static double share_difficulty_leading_zeros(const uint8_t hash[32]) {
    // Count leading zero bytes/bits
    int leading_zeros = 0;
    for (int i = 31; i >= 0; i--) {  // hash is little-endian, so MSB is at [31]
        if (hash[i] == 0) {
            leading_zeros += 8;
        } else {
            // Count leading zeros in this byte
            for (int b = 7; b >= 0; b--) {
                if (hash[i] & (1 << b)) break;
                leading_zeros++;
            }
            break;
        }
    }
    // Each additional bit of leading zeros doubles the difficulty
    return pow(2.0, leading_zeros) / 65536.0;  // normalized to diff1
}

int main() {
    // Hash from Method 2 (the one with leading zeros)
    uint8_t hash_method2[32];
    const char* hex = "916061410db5732ce371e33ba056b3cdfedeb0582e74c2777ac8e05d2b980200";
    for (int i = 0; i < 32; i++) {
        unsigned int b;
        sscanf(hex + i*2, "%2x", &b);
        hash_method2[i] = b;
    }
    
    printf("Hash (raw bytes): ");
    for (int i = 0; i < 32; i++) printf("%02x", hash_method2[i]);
    printf("\n");
    
    printf("Hash (reversed for display): ");
    for (int i = 31; i >= 0; i--) printf("%02x", hash_method2[i]);
    printf("\n\n");
    
    double diff1 = share_difficulty_from_hash(hash_method2);
    double diff2 = share_difficulty_leading_zeros(hash_method2);
    
    printf("Difficulty (standard method):       %.12e\n", diff1);
    printf("Difficulty (leading zeros method):  %.12e\n", diff2);
    printf("Pool difficulty:                    0.1\n\n");
    
    // What hash would be needed for diff 0.1?
    printf("For difficulty 0.1, target hash would start with:\n");
    double target_val = ldexp((double)0xffff0000, 192) / 0.1;
    printf("  Target value: %.6e\n", target_val);
    
    // Calculate how many leading zero bits are needed
    int bits_needed = (int)(log2(target_val) - log2(256.0) * 32);
    printf("  Approximately %d leading zero bits needed\n", 256 - bits_needed);
    
    return 0;
}
