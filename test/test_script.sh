#!/bin/bash

# Test script for stratum proxy project
# Hardcoded steps for each coin core config

cd /home/tomas_admin/stratum-proxy

# For coin_core_repo_bitcoinII_main.conf
clear
cp coin_core_samples/coin_core_repo_bitcoinII_main.conf coin_core_repo.conf
rm -rf coin_core
scripts/clone_coin_core.sh
scripts/build_coin_core.sh
make -j$(nproc)
read -p "Press enter to continue to next config"

# For coin_core_repo_bitcoin_28.conf
clear
cp coin_core_samples/coin_core_repo_bitcoin_28.conf coin_core_repo.conf
rm -rf coin_core
make clean
scripts/clone_coin_core.sh
scripts/build_coin_core.sh
make -j$(nproc)
read -p "Press enter to continue to next config"

# For coin_core_repo_fixedcoin_v29.conf
clear
cp coin_core_samples/coin_core_repo_fixedcoin_v29.conf coin_core_repo.conf
rm -rf coin_core
make clean
scripts/clone_coin_core.sh
scripts/build_coin_core.sh
make -j$(nproc)
read -p "Press enter to continue to next config"

# For coin_core_repo_rincoin_master.conf
clear
cp coin_core_samples/coin_core_repo_rincoin_master.conf coin_core_repo.conf
rm -rf coin_core
make clean
scripts/clone_coin_core.sh
scripts/build_coin_core.sh
make -j$(nproc)
read -p "Press enter to continue to next config"

# For coin_core_repo_rincoin_takologi102.conf
clear
cp coin_core_samples/coin_core_repo_rincoin_takologi102.conf coin_core_repo.conf
rm -rf coin_core
make clean
scripts/clone_coin_core.sh
scripts/build_coin_core.sh
make -j$(nproc)
read -p "Press enter to continue to next config"

# For coin_core_repo_yenten_yenten6.conf
cp coin_core_samples/coin_core_repo_yenten_yenten6.conf coin_core_repo.conf
rm -rf coin_core
make clean
scripts/clone_coin_core.sh
scripts/build_coin_core.sh
make -j$(nproc)
read -p "Press enter to continue to next config"