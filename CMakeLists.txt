cmake_minimum_required(VERSION 3.10)
project(miband_c C)

set(CMAKE_C_STANDARD 11)

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

add_executable(miband_c main.c band.c)

target_link_libraries(miband_c OpenSSL::SSL OpenSSL::Crypto)

# Link tiny-ecdh-c
set(TINY_ECDH_C_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/../tiny-ECDH-c")
set(TINY_ECDH_C_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/tiny-ECDH-c")
add_subdirectory(${TINY_ECDH_C_SRC_DIR} ${TINY_ECDH_C_BIN_DIR})
target_link_libraries(miband_c tiny-ECDH-c)

# Find gattlib
find_package(PkgConfig REQUIRED)
pkg_check_modules(GATTLIB REQUIRED gattlib)

# Link gattlib to your main executable
target_link_libraries(miband_c ${GATTLIB_LIBRARIES})

# Include gattlib headers
target_include_directories(miband_c PRIVATE ${GATTLIB_INCLUDE_DIRS})

# Set a default value for MAC_ADDRESS
set(MAC_ADDRESS "FF:FF:FF:FF:FF:FF" CACHE STRING "MAC address of the BLE device")

# Pass the MAC_ADDRESS to the compilation process as a preprocessor definition
add_definitions(-DMAC_ADDRESS="${MAC_ADDRESS}")

# Set a default value for BAND_TYPE
set(BAND_TYPE "6" CACHE STRING "Band Model")

# Pass the BAND_TYPE to the compilation process as a preprocessor definition
add_definitions(-DBAND_TYPE="${BAND_TYPE}")

add_compile_definitions(AUTH_KEY_FILE="${CMAKE_CURRENT_SOURCE_DIR}/auth_key.txt")