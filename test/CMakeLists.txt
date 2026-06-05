cmake_minimum_required(VERSION 3.16)
project(mp-customdata-test VERSION 0.1.0 LANGUAGES C)

# CustomData-C package path (supplied by Nix cmakeFlags)
set(CUSTOMDATA_C_DIR "" CACHE PATH "Path to customdata-c package output")

# Collect all C sources from customdata-c
file(GLOB CUSTOMDATA_C_SRCS "${CUSTOMDATA_C_DIR}/src/*.c")

# Test executable
add_executable(mp-customdata-test
    main.c
    ${CUSTOMDATA_C_SRCS}
)

# Include paths
target_include_directories(mp-customdata-test PRIVATE
    ${CUSTOMDATA_C_DIR}/include
)

# C11
target_compile_features(mp-customdata-test PRIVATE c_std_11)