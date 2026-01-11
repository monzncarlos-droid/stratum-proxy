#!/usr/bin/env python3
"""
Extract powlimit_bits from a coin core's chainparams.cpp

powlimit_bits is the number of leading zeros in the mainnet powLimit,
which is used by mining pools (like yiimp) for difficulty calculations.

Usage: extract_powlimit_bits.py <path_to_coin_core>
Output: Prints the powlimit_bits value (integer)
"""

import sys
import re
import os

def find_chainparams(coin_core_path):
    """Find chainparams.cpp in various possible locations."""
    candidates = [
        os.path.join(coin_core_path, 'src', 'kernel', 'chainparams.cpp'),  # Newer Bitcoin
        os.path.join(coin_core_path, 'src', 'chainparams.cpp'),  # Most forks
    ]
    # Return first that exists
    for path in candidates:
        if os.path.exists(path):
            return path
    return None

def extract_mainnet_powlimit(content):
    """Extract mainnet powLimit from chainparams.cpp content."""
    
    # Try different patterns used by various Bitcoin forks
    
    # Pattern 1: CMainParams() constructor with consensus.powLimit = uint256S("...")
    match = re.search(
        r'CMainParams\s*\(\s*\).*?consensus\.powLimit\s*=\s*uint256S?\s*[\(\{]\s*["\']?([^"\'}\)]+)["\']?\s*[\)\}]',
        content, re.DOTALL
    )
    if match:
        return match.group(1)
    
    # Pattern 2: consensus.powLimit = uint256{"..."} (newer Bitcoin style) - first occurrence for mainnet
    match = re.search(
        r'consensus\.powLimit\s*=\s*uint256\s*\{\s*"([^"]+)"\s*\}',
        content
    )
    if match:
        return match.group(1)
    
    # Pattern 3: consensus.powLimit = uint256S("...")
    match = re.search(
        r'consensus\.powLimit\s*=\s*uint256S\s*\(\s*"([^"]+)"\s*\)',
        content
    )
    if match:
        return match.group(1)
    
    return None

def hex_to_leading_zeros(hex_str):
    """Calculate number of leading zeros in a 256-bit hex value."""
    # Remove 0x prefix if present
    hex_str = hex_str.replace('0x', '').replace('0X', '')
    
    # Pad to 64 hex characters (256 bits)
    hex_str = hex_str.zfill(64)
    
    # Convert to integer
    value = int(hex_str, 16)
    
    if value == 0:
        return 256
    
    # Count leading zeros: 256 - bit_length
    return 256 - value.bit_length()

def main():
    if len(sys.argv) != 2:
        print("Usage: extract_powlimit_bits.py <path_to_coin_core>", file=sys.stderr)
        sys.exit(1)
    
    coin_core_path = sys.argv[1]
    
    chainparams_path = find_chainparams(coin_core_path)
    if not chainparams_path:
        print(f"Error: Could not find chainparams.cpp in {coin_core_path}", file=sys.stderr)
        sys.exit(1)
    
    with open(chainparams_path, 'r') as f:
        content = f.read()
    
    powlimit_hex = extract_mainnet_powlimit(content)
    if not powlimit_hex:
        print(f"Error: Could not extract powLimit from {chainparams_path}", file=sys.stderr)
        # Default to Bitcoin's 32 bits
        print("32")
        sys.exit(0)
    
    leading_zeros = hex_to_leading_zeros(powlimit_hex)
    print(leading_zeros)

if __name__ == '__main__':
    main()
