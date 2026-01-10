# CMakeLists.txt for stratum-proxy
# Universal stratum proxy for Bitcoin/Litecoin-forked coins.
# The hash function is provided by the coin core cloned/built in coin_core/.

cmake_minimum_required(VERSION 3.16)
project(stratum_proxy LANGUAGES C CXX)

# -----------------------------------------------------------------------------
# Configuration file reading (early - we need CXX_STANDARD)
# -----------------------------------------------------------------------------
set(COIN_CORE_CONF "${CMAKE_SOURCE_DIR}/coin_core_repo.conf")
if(EXISTS "${COIN_CORE_CONF}")
    file(READ "${COIN_CORE_CONF}" CONF_CONTENTS)
    string(REPLACE "\n" ";" CONF_LINES "${CONF_CONTENTS}")
    
    # Read CXX standard first (default: 20)
    set(COIN_CORE_CXX_STANDARD "20")
    foreach(LINE ${CONF_LINES})
        if(LINE MATCHES "^COIN_CORE_CXX_STANDARD=(.*)$")
            string(STRIP "${CMAKE_MATCH_1}" COIN_CORE_CXX_STANDARD)
        endif()
    endforeach()
endif()

# C++ standard - modern cores need C++20, older forks may need C++17
set(CMAKE_CXX_STANDARD ${COIN_CORE_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
message(STATUS "Using C++ standard: ${CMAKE_CXX_STANDARD}")

# -----------------------------------------------------------------------------
# Configuration: Coin Core paths
# -----------------------------------------------------------------------------
# The coin core should be cloned and built in ${CMAKE_SOURCE_DIR}/coin_core
# before running cmake. Use scripts/clone_coin_core.sh and scripts/build_coin_core.sh.

set(COIN_CORE_DIR "${CMAKE_SOURCE_DIR}/coin_core")
set(COIN_CORE_SRC "${COIN_CORE_DIR}/src")

# Check if coin_core exists
if(NOT EXISTS "${COIN_CORE_SRC}")
    message(FATAL_ERROR
        "Coin core source not found at ${COIN_CORE_SRC}.\n"
        "Please run:\n"
        "  1. Edit coin_core_repo.conf to set the COIN_CORE_REPO_URL\n"
        "  2. ./scripts/clone_coin_core.sh\n"
        "  3. ./scripts/build_coin_core.sh\n"
        "Then re-run cmake."
    )
endif()

# -----------------------------------------------------------------------------
# Find required packages
# -----------------------------------------------------------------------------
find_package(OpenSSL REQUIRED)

# Find Boost (required by coin cores)
# Components: filesystem is commonly needed by libbitcoin_util
find_package(Boost REQUIRED COMPONENTS filesystem)

# -----------------------------------------------------------------------------
# tomlplusplus (header-only TOML parser)
# -----------------------------------------------------------------------------
set(TOMLPLUSPLUS_DIR "${CMAKE_SOURCE_DIR}/extern/tomlplusplus")
if(NOT EXISTS "${TOMLPLUSPLUS_DIR}/include/toml++/toml.hpp")
    message(FATAL_ERROR
        "tomlplusplus not found at ${TOMLPLUSPLUS_DIR}.\n"
        "Please run: git clone --depth 1 https://github.com/marzer/tomlplusplus.git extern/tomlplusplus"
    )
endif()

# -----------------------------------------------------------------------------
# Coin core include directories (common across Bitcoin/Litecoin forks)
# -----------------------------------------------------------------------------
set(COIN_CORE_INCLUDES
    ${COIN_CORE_SRC}
    ${COIN_CORE_SRC}/crypto
    ${COIN_CORE_SRC}/secp256k1/include
    ${COIN_CORE_DIR}/build
)

# Try to find config/bitcoin-config.h or similar
if(EXISTS "${COIN_CORE_DIR}/build/src/config")
    list(APPEND COIN_CORE_INCLUDES "${COIN_CORE_DIR}/build/src/config")
endif()
if(EXISTS "${COIN_CORE_SRC}/config")
    list(APPEND COIN_CORE_INCLUDES "${COIN_CORE_SRC}/config")
endif()

# Read configuration from coin_core_repo.conf
set(COIN_CORE_CONF "${CMAKE_SOURCE_DIR}/coin_core_repo.conf")
set(COIN_HASH_FUNCTION "GetPoWHash")  # Default for most altcoins
if(EXISTS "${COIN_CORE_CONF}")
    file(STRINGS "${COIN_CORE_CONF}" CONF_LINES)
    foreach(LINE ${CONF_LINES})
        # Read hash function name
        if(LINE MATCHES "^COIN_CORE_HASH_FUNCTION=(.*)$")
            string(STRIP "${CMAKE_MATCH_1}" COIN_HASH_FUNCTION)
            message(STATUS "Using hash function: ${COIN_HASH_FUNCTION}")
        endif()
        # Read extra include paths
        if(LINE MATCHES "^COIN_CORE_EXTRA_INCLUDES=(.*)$")
            string(STRIP "${CMAKE_MATCH_1}" EXTRA_INCLUDES)
            # Split by spaces and add each path
            string(REPLACE " " ";" EXTRA_INCLUDE_LIST "${EXTRA_INCLUDES}")
            foreach(EXTRA_INC ${EXTRA_INCLUDE_LIST})
                if(NOT EXTRA_INC STREQUAL "")
                    set(FULL_PATH "${COIN_CORE_SRC}/${EXTRA_INC}")
                    if(EXISTS "${FULL_PATH}")
                        list(APPEND COIN_CORE_INCLUDES "${FULL_PATH}")
                        message(STATUS "Added extra include path: ${FULL_PATH}")
                    else()
                        message(WARNING "Extra include path not found: ${FULL_PATH}")
                    endif()
                endif()
            endforeach()
        endif()
    endforeach()
endif()

# Pass hash function name to compiler
add_compile_definitions(COIN_HASH_FUNCTION=${COIN_HASH_FUNCTION})

# -----------------------------------------------------------------------------
# Coin core library paths (from config)
# -----------------------------------------------------------------------------
# Read COIN_CORE_LIB_DIR, COIN_CORE_LIB_PREFIX, and COIN_CORE_BINARY_SUFFIX from config
set(COIN_CORE_LIB_DIR "")
set(COIN_CORE_LIB_PREFIX "")
set(COIN_CORE_BINARY_SUFFIX "")
if(EXISTS "${COIN_CORE_CONF}")
    foreach(LINE ${CONF_LINES})
        if(LINE MATCHES "^COIN_CORE_LIB_DIR=(.*)$")
            string(STRIP "${CMAKE_MATCH_1}" COIN_CORE_LIB_DIR_REL)
            if(NOT COIN_CORE_LIB_DIR_REL STREQUAL "")
                set(COIN_CORE_LIB_DIR "${COIN_CORE_DIR}/${COIN_CORE_LIB_DIR_REL}")
                message(STATUS "Using library directory: ${COIN_CORE_LIB_DIR}")
            endif()
        endif()
        if(LINE MATCHES "^COIN_CORE_LIB_PREFIX=(.*)$")
            string(STRIP "${CMAKE_MATCH_1}" COIN_CORE_LIB_PREFIX)
            if(NOT COIN_CORE_LIB_PREFIX STREQUAL "")
                message(STATUS "Using library prefix: ${COIN_CORE_LIB_PREFIX}")
            endif()
        endif()
        if(LINE MATCHES "^COIN_CORE_BINARY_SUFFIX=(.*)$")
            string(STRIP "${CMAKE_MATCH_1}" COIN_CORE_BINARY_SUFFIX)
            if(NOT COIN_CORE_BINARY_SUFFIX STREQUAL "")
                message(STATUS "Using binary suffix: ${COIN_CORE_BINARY_SUFFIX}")
            endif()
        endif()
    endforeach()
endif()

# If no explicit lib dir, use default paths
if(COIN_CORE_LIB_DIR STREQUAL "")
    set(COIN_CORE_LIB_DIR "${COIN_CORE_SRC}")
    message(STATUS "Using default library directory: ${COIN_CORE_LIB_DIR}")
endif()

# If no explicit prefix, use "bitcoin" as default
if(COIN_CORE_LIB_PREFIX STREQUAL "")
    set(COIN_CORE_LIB_PREFIX "bitcoin")
endif()

# Build library names from prefix
set(LIB_NAME_CONSENSUS "${COIN_CORE_LIB_PREFIX}_consensus")
set(LIB_NAME_CRYPTO "${COIN_CORE_LIB_PREFIX}_crypto")
set(LIB_NAME_UTIL "${COIN_CORE_LIB_PREFIX}_util")
# Older forks use single unified library: lib{prefix}consensus.a
set(LIB_NAME_CONSENSUS_UNIFIED "${COIN_CORE_LIB_PREFIX}consensus")

# Find required libraries
# Try modern naming first ({prefix}_consensus), then unified ({prefix}consensus)
find_library(LIB_CONSENSUS
    NAMES ${LIB_NAME_CONSENSUS} ${LIB_NAME_CONSENSUS_UNIFIED}
    HINTS ${COIN_CORE_LIB_DIR}
    NO_DEFAULT_PATH
)

find_library(LIB_CRYPTO
    NAMES ${LIB_NAME_CRYPTO} ${COIN_CORE_LIB_PREFIX}_crypto_base
    HINTS ${COIN_CORE_LIB_DIR}
          ${COIN_CORE_LIB_DIR}/crypto/.libs
    NO_DEFAULT_PATH
)

# Optional: CPU-optimized crypto libraries (Bitcoin Core v28+)
find_library(LIB_CRYPTO_SSE41
    NAMES ${COIN_CORE_LIB_PREFIX}_crypto_sse41
    HINTS ${COIN_CORE_LIB_DIR}
          ${COIN_CORE_LIB_DIR}/crypto/.libs
    NO_DEFAULT_PATH
)
find_library(LIB_CRYPTO_AVX2
    NAMES ${COIN_CORE_LIB_PREFIX}_crypto_avx2
    HINTS ${COIN_CORE_LIB_DIR}
          ${COIN_CORE_LIB_DIR}/crypto/.libs
    NO_DEFAULT_PATH
)
find_library(LIB_CRYPTO_SHANI
    NAMES ${COIN_CORE_LIB_PREFIX}_crypto_x86_shani
    HINTS ${COIN_CORE_LIB_DIR}
          ${COIN_CORE_LIB_DIR}/crypto/.libs
    NO_DEFAULT_PATH
)

# Look for secp256k1 in main lib dir and common autotools location
find_library(LIB_SECP256K1
    NAMES secp256k1
    HINTS ${COIN_CORE_LIB_DIR}
          ${CMAKE_CURRENT_LIST_DIR}/coin_core/src/secp256k1/.libs
    NO_DEFAULT_PATH
)

find_library(LIB_UTIL
    NAMES ${LIB_NAME_UTIL}
    HINTS ${COIN_CORE_LIB_DIR}
    NO_DEFAULT_PATH
)

# -----------------------------------------------------------------------------
# Extra libraries from coin_core_repo.conf (for coin-specific dependencies)
# -----------------------------------------------------------------------------
# Read COIN_CORE_EXTRA_LIB_DIRS and COIN_CORE_EXTRA_LIBS from config file
set(EXTRA_LIB_DIRS_LIST "")
set(EXTRA_LIBS_LIST "")
if(EXISTS "${COIN_CORE_CONF}")
    foreach(LINE ${CONF_LINES})
        if(LINE MATCHES "^COIN_CORE_EXTRA_LIB_DIRS=(.*)$")
            string(STRIP "${CMAKE_MATCH_1}" EXTRA_LIB_DIRS)
            string(REPLACE " " ";" EXTRA_LIB_DIRS_LIST "${EXTRA_LIB_DIRS}")
        endif()
        if(LINE MATCHES "^COIN_CORE_EXTRA_LIBS=(.*)$")
            string(STRIP "${CMAKE_MATCH_1}" EXTRA_LIBS)
            string(REPLACE " " ";" EXTRA_LIBS_LIST "${EXTRA_LIBS}")
        endif()
    endforeach()
endif()

# Build full paths for extra lib dirs
set(EXTRA_LIB_HINTS "")
foreach(EXTRA_DIR ${EXTRA_LIB_DIRS_LIST})
    if(NOT EXTRA_DIR STREQUAL "")
        list(APPEND EXTRA_LIB_HINTS "${CMAKE_CURRENT_LIST_DIR}/coin_core/${EXTRA_DIR}")
        message(STATUS "Added extra lib dir: ${CMAKE_CURRENT_LIST_DIR}/coin_core/${EXTRA_DIR}")
    endif()
endforeach()

# -----------------------------------------------------------------------------
# Build stratum-proxy executable
# -----------------------------------------------------------------------------
add_executable(stratum-proxy
    src/main.cpp
    src/coin_hash_wrapper.cpp
)

# Set output name with optional suffix (e.g., stratum-proxy_rincoin)
if(NOT COIN_CORE_BINARY_SUFFIX STREQUAL "")
    set_target_properties(stratum-proxy PROPERTIES OUTPUT_NAME "stratum-proxy-${COIN_CORE_BINARY_SUFFIX}")
    message(STATUS "Output binary: stratum-proxy-${COIN_CORE_BINARY_SUFFIX}")
endif()

target_include_directories(stratum-proxy PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${COIN_CORE_INCLUDES}
    ${TOMLPLUSPLUS_DIR}/include
)

target_link_libraries(stratum-proxy PRIVATE
    OpenSSL::Crypto
    Boost::boost
)

# Link coin core libraries if found
if(LIB_CONSENSUS)
    target_link_libraries(stratum-proxy PRIVATE ${LIB_CONSENSUS})
    message(STATUS "Found consensus library: ${LIB_CONSENSUS}")
else()
    message(FATAL_ERROR "Consensus library not found. Check COIN_CORE_LIB_DIR and COIN_CORE_LIB_PREFIX in coin_core_repo.conf")
endif()

# Crypto library is separate in modern cores, but included in consensus for older forks
if(LIB_CRYPTO)
    target_link_libraries(stratum-proxy PRIVATE ${LIB_CRYPTO})
    message(STATUS "Found crypto library: ${LIB_CRYPTO}")
else()
    message(STATUS "Crypto library not found (may be included in consensus library)")
endif()

# Link additional crypto libraries (Bitcoin Core v28+ splits these by CPU feature)
if(LIB_CRYPTO_SSE41)
    target_link_libraries(stratum-proxy PRIVATE ${LIB_CRYPTO_SSE41})
    message(STATUS "Found crypto sse41 library: ${LIB_CRYPTO_SSE41}")
endif()
if(LIB_CRYPTO_AVX2)
    target_link_libraries(stratum-proxy PRIVATE ${LIB_CRYPTO_AVX2})
    message(STATUS "Found crypto avx2 library: ${LIB_CRYPTO_AVX2}")
endif()
if(LIB_CRYPTO_SHANI)
    target_link_libraries(stratum-proxy PRIVATE ${LIB_CRYPTO_SHANI})
    message(STATUS "Found crypto shani library: ${LIB_CRYPTO_SHANI}")
endif()

if(LIB_UTIL)
    target_link_libraries(stratum-proxy PRIVATE ${LIB_UTIL})
    message(STATUS "Found util library: ${LIB_UTIL}")
endif()

if(LIB_SECP256K1)
    target_link_libraries(stratum-proxy PRIVATE ${LIB_SECP256K1})
    message(STATUS "Found secp256k1 library: ${LIB_SECP256K1}")
endif()

# Link extra libraries from COIN_CORE_EXTRA_LIBS config
foreach(EXTRA_LIB ${EXTRA_LIBS_LIST})
    if(NOT EXTRA_LIB STREQUAL "")
        find_library(LIB_EXTRA_${EXTRA_LIB}
            NAMES ${EXTRA_LIB}
            HINTS ${COIN_CORE_LIB_DIR} ${EXTRA_LIB_HINTS}
            NO_DEFAULT_PATH
        )
        if(LIB_EXTRA_${EXTRA_LIB})
            target_link_libraries(stratum-proxy PRIVATE ${LIB_EXTRA_${EXTRA_LIB}})
            message(STATUS "Found extra library: ${LIB_EXTRA_${EXTRA_LIB}}")
        else()
            message(FATAL_ERROR "Extra library not found: ${EXTRA_LIB}\nSearched in: ${COIN_CORE_LIB_DIR} ${EXTRA_LIB_HINTS}")
        endif()
    endif()
endforeach()

# Platform-specific settings
if(UNIX)
    target_compile_options(stratum-proxy PRIVATE -Wall -Wextra)
    target_link_libraries(stratum-proxy PRIVATE pthread)
endif()

# Link Boost::filesystem last (needed by libbitcoin_util)
target_link_libraries(stratum-proxy PRIVATE Boost::filesystem)

# -----------------------------------------------------------------------------
# Installation
# -----------------------------------------------------------------------------
install(TARGETS stratum-proxy DESTINATION bin)
