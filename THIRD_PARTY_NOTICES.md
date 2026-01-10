# Third-party notices

This project vendors and/or depends on third-party software. The following notices are provided for attribution and license compliance.

## Vendored into this repository

### tnn-miner (RinHash and related glue)

- Upstream: https://github.com/Tritonn204/tnn-miner
- License: MIT
- Used for: RinHash implementation and supporting crypto glue

### BLAKE3 (C implementation)

- Upstream: https://github.com/BLAKE3-team/BLAKE3
- License: CC0 1.0 Universal (Public Domain Dedication).
- Vendored files: `extern/BLAKE3/c/*`
- License text included:
  - `licenses/BLAKE3_LICENSE_CC0.txt`

### Argon2 / Blake2 (RandomX/tevador portable subset)

- Upstream (RandomX): https://github.com/tevador/RandomX
- License: 3-clause BSD-style (see `licenses/TEVADOR_BSD3.txt`)
- Notes: these files include a notice that parts originate from the Argon2 reference code (CC0): https://github.com/P-H-C/phc-winner-argon2

### libkeccak-tiny (tiny-keccak)

- Upstream: https://github.com/coruus/keccak-tiny (commonly distributed as "libkeccak-tiny")
- Implementor: David Leon Gil
- License: CC0 (as stated in the file header)
- Vendored file: `src/crypto/tiny-keccak/tiny-keccak.h`

## External (system) dependencies

These are *not* vendored here; they are provided by your system/toolchain.

### OpenSSL

- Used for: `libcrypto` hashing primitives
- License: OpenSSL License (varies by version; see your installed OpenSSL distribution)

### Boost (headers)

- Used for: multiprecision arithmetic (`boost::multiprecision`)
- License: Boost Software License 1.0 (see your installed Boost distribution)
