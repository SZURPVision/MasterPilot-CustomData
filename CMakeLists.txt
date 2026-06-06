cmake_minimum_required(VERSION 3.16)
project(MasterPilot-CustomData VERSION 0.1.0 LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ── 构建选项 ──────────────────────────────────────────
option(BUILD_TESTING "Build C test executable" OFF)
option(BUILD_CPP    "Build C++ wrapper library" OFF)

# ── 依赖路径（由 Nix cmakeFlags 注入；纯 CMake 环境下可手动设） ──
#   CUSTOMDATA_SRC_DIR   指向 buf generate 生成的源代码目录（含 c/ cpp/ 等子目录）
#   NANOPB_DIR           指向 nanopb 安装前缀（可选，通常 find_package 即可解决）
set(CUSTOMDATA_SRC_DIR "" CACHE PATH
    "Path to buf-generated proto sources (customdata-src package output)")
set(NANOPB_DIR "" CACHE PATH
    "Path to nanopb installation prefix (optional)")
set(PROROBUF_DIR "" CACHE PATH
    "Path to protobuf installation prefix (optional)")

# ── C 源码目录（始终为项目内的 src/） ──
set(MPC_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

# ── 子目录 ────────────────────────────────────────────
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()

if(BUILD_CPP)
    add_subdirectory(cpp)
endif()