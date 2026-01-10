#!/bin/bash
# clone_coin_core.sh
# Usage: ./clone_coin_core.sh
# Reads configuration from ../coin_core_repo.conf and clones/copies the repo into ../coin_core
# Does nothing if coin_core already exists.

set -e
cd "$(dirname "$0")/.."

# Check if coin_core already exists
if [ -d "coin_core" ]; then
  echo "coin_core directory already exists, skipping clone."
  echo "To re-clone, remove the directory first: rm -rf coin_core"
  exit 0
fi

# Read configuration from coin_core_repo.conf
if [ ! -f coin_core_repo.conf ]; then
  echo "coin_core_repo.conf not found!"
  exit 1
fi

# Parse config file
REPO_URL=$(grep '^COIN_CORE_REPO_URL=' coin_core_repo.conf | cut -d'=' -f2- | tr -d '\r' | xargs)
BRANCH=$(grep '^COIN_CORE_BRANCH=' coin_core_repo.conf | cut -d'=' -f2- | tr -d '\r' | xargs)

if [ -z "$REPO_URL" ]; then
  echo "COIN_CORE_REPO_URL not set in coin_core_repo.conf"
  exit 1
fi

# Check if it's a local path or a git URL
if [ -d "$REPO_URL" ]; then
  echo "Copying local coin core from $REPO_URL..."
  cp -a "$REPO_URL" coin_core
  echo "Local coin core copied (build artifacts preserved)."
  
  # Checkout specific branch if specified
  if [ -n "$BRANCH" ]; then
    echo "Checking out branch/tag: $BRANCH"
    cd coin_core
    git checkout "$BRANCH"
    cd ..
  fi
else
  echo "Cloning coin core from $REPO_URL..."
  if [ -n "$BRANCH" ]; then
    echo "Using branch/tag: $BRANCH (single-branch clone)"
    git clone --branch "$BRANCH" --single-branch --depth 1 "$REPO_URL" coin_core
  else
    echo "No branch specified, cloning default branch (shallow)"
    git clone --depth 1 "$REPO_URL" coin_core
  fi
  echo "Coin core cloned."
fi
