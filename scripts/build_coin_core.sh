#!/bin/bash
# build_coin_core.sh
# Usage: ./build_coin_core.sh
# Builds the coin core cloned in ../coin_core with configuration from ../coin_core_repo.conf.
# Only builds what's needed to get the consensus/crypto libraries for hash computation.

set -e
cd "$(dirname "$0")/.."

if [ ! -d coin_core ]; then
  echo "coin_core directory does not exist. Run clone_coin_core.sh first."
  exit 1
fi

# Read configuration from coin_core_repo.conf
CONFIGURE_OPTS=""
if [ -f coin_core_repo.conf ]; then
  CONFIGURE_OPTS=$(grep '^COIN_CORE_CONFIGURE_OPTS=' coin_core_repo.conf | cut -d'=' -f2- | tr -d '\r')
fi

# Default configure options if not specified
if [ -z "$CONFIGURE_OPTS" ]; then
  CONFIGURE_OPTS="--disable-wallet --disable-tests --disable-bench --disable-zmq --without-gui --with-daemon=no --with-utils=no"
  echo "Using default configure options: $CONFIGURE_OPTS"
else
  echo "Using configure options from config: $CONFIGURE_OPTS"
fi

cd coin_core

# Check if already built (look for libbitcoinconsensus or similar)
if ls src/.libs/lib*consensus*.a 2>/dev/null | head -1 | grep -q .; then
  echo "Coin core libraries already built, skipping."
  echo "To rebuild, run: cd coin_core && make clean && cd .. && make build-core"
  exit 0
fi

# Initialize and update git submodules if any
if [ -f .gitmodules ]; then
  echo "Initializing git submodules..."
  git submodule update --init --recursive
fi

# Try to detect build system and build with minimal configuration
# We only need the consensus/crypto libraries for hash computation.

if [ -f autogen.sh ] && [ ! -f configure ]; then
  echo "Running autogen.sh..."
  ./autogen.sh
fi

if [ -f configure ]; then
  echo "Running configure..."
  # shellcheck disable=SC2086
  ./configure $CONFIGURE_OPTS
fi

if [ -f Makefile ]; then
  echo "Building coin core (this may take a while)..."
  # Only build the consensus library if possible, otherwise build all
  if make -n src/libbitcoinconsensus.la >/dev/null 2>&1; then
    make -j$(nproc) src/libbitcoinconsensus.la
  else
    make -j$(nproc)
  fi
  echo "Coin core build complete."
elif [ -f CMakeLists.txt ]; then
  mkdir -p build && cd build
  cmake .. -DBUILD_TESTING=OFF -DENABLE_WALLET=OFF
  make -j$(nproc)
else
  echo "No supported build system found."
  exit 2
fi
