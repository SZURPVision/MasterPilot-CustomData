cmake_minimum_required(VERSION 3.16)
project(MasterPilot-CustomData VERSION 0.1.0 LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ── 外部可覆盖路径 (Nix 通过 -DXXXX_DIR= 显式传入，否则回退为项目相对路径) ──
if(NOT DEFINED CUSTOMDATA_GENERATED_DIR)
    set(CUSTOMDATA_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated"
        CACHE PATH "Path to buf-generated protobuf files")
endif()

# ── 内部路径 ──
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

# ── 构建开关 ──
option(MINIMAL_MODE "Only build what is explicitly enabled" OFF)
if(MINIMAL_MODE)
    set(_default_build OFF)
else()
    set(_default_build ON)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()
option(BUILD_NANOPB "Build nanopb encode/decode layer" ${_default_build})
option(BUILD_CPP    "Build C++ protobuf wrapper"       ${_default_build})
option(BUILD_TOOLS  "Build CLI tools"                  ${_default_build})


add_subdirectory(src/core)

if(BUILD_NANOPB)
    add_subdirectory(src/nanopb)
endif()

if(BUILD_CPP)
    add_subdirectory(src/cpp)
endif()

if(BUILD_TOOLS)
    add_subdirectory(src/tools)
endif()
