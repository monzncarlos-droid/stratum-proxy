// Test YesPower difficulty calculation (different diff1)
#include <cstdio>
#include <cstdint>
#include <cmath>

// Standard Bitcoin difficulty (diff1 = 0xffff0000 * 2^192)
static double btc_difficulty(const uint8_t hash[32]) {
    double diff1 = ldexp((double)0xffff0000, 192);
    double h = 0;
    for (int i = 31; i >= 0; --i) {
        h = h * 256.0 + hash[i];
    }
    if (h == 0) return 1e30;
    return diff1 / h;
}

// YesPower difficulty (diff1 = 2^256 / 2^16 = 2^240)
static double yespower_difficulty(const uint8_t hash[32]) {
    double diff1 = ldexp(1.0, 240);  // 2^240
    double h = 0;
    for (int i = 31; i >= 0; --i) {
        h = h * 256.0 + hash[i];
    }
    if (h == 0) return 1e30;
    return diff1 / h;
}

// Alternative YesPower difficulty (diff1 = 0xffff * 2^208)
static double yespower_difficulty_v2(const uint8_t hash[32]) {
    double diff1 = ldexp((double)0xffff, 208);
    double h = 0;
    for (int i = 31; i >= 0; --i) {
        h = h * 256.0 + hash[i];
    }
    if (h == 0) return 1e30;
    return diff1 / h;
}

// Network difficulty style (common for YesPower pools)
static double network_difficulty(const uint8_t hash[32]) {
    // diff = 2^256 / hash / 2^32
    double h = 0;
    for (int i = 31; i >= 0; --i) {
        h = h * 256.0 + hash[i];
    }
    if (h == 0) return 1e30;
    return ldexp(1.0, 256) / h / ldexp(1.0, 32);
}

int main() {
    // Hash from Method 2 (the one with leading zeros) - raw bytes
    uint8_t hash[32];
    const char* hex = "916061410db5732ce371e33ba056b3cdfedeb0582e74c2777ac8e05d2b980200";
    for (int i = 0; i < 32; i++) {
        unsigned int b;
        sscanf(hex + i*2, "%2x", &b);
        hash[i] = b;
    }
    
    printf("Hash (raw LE bytes): %s\n", hex);
    printf("Hash (display BE):   ");
    for (int i = 31; i >= 0; i--) printf("%02x", hash[i]);
    printf("\n\n");
    
    printf("Pool difficulty target: 0.1\n\n");
    
    printf("Different difficulty calculations:\n");
    printf("  Bitcoin standard (0xffff0000 * 2^192): %.12e\n", btc_difficulty(hash));
    printf("  YesPower v1 (2^240):                   %.12e\n", yespower_difficulty(hash));
    printf("  YesPower v2 (0xffff * 2^208):          %.12e\n", yespower_difficulty_v2(hash));
    printf("  Network difficulty (2^256/h/2^32):    %.12e\n", network_difficulty(hash));
    
    // Calculate what we'd need for each to meet 0.1
    printf("\nDoes hash meet diff 0.1?\n");
    printf("  Bitcoin:     %s\n", btc_difficulty(hash) >= 0.1 ? "YES" : "NO");
    printf("  YesPower v1: %s\n", yespower_difficulty(hash) >= 0.1 ? "YES" : "NO");
    printf("  YesPower v2: %s\n", yespower_difficulty_v2(hash) >= 0.1 ? "YES" : "NO");
    printf("  Network:     %s\n", network_difficulty(hash) >= 0.1 ? "YES" : "NO");
    
    return 0;
}
