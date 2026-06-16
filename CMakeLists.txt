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
option(BUILD_EMBEDDED "Build embedded wrapper with nanopb" ${_default_build})
option(BUILD_CPP    "Build C++ protobuf wrapper"       ${_default_build})
option(BUILD_TOOLS  "Build CLI tools"                  ${_default_build})


# ── 构建虚拟 include 目录（符号链接，不改变源码树物理结构）──
set(MP_VIRTUAL_INC "${CMAKE_BINARY_DIR}/include/masterpilot")
file(MAKE_DIRECTORY "${MP_VIRTUAL_INC}")

file(GLOB MP_CORE_HEADERS "${CMAKE_SOURCE_DIR}/src/core/*.h")
file(GLOB MP_NANOPB_HEADERS "${CMAKE_SOURCE_DIR}/src/embedded/*.h")
file(GLOB MP_CPP_HEADERS "${CMAKE_SOURCE_DIR}/src/cpp/*.hpp")

foreach(_hdr IN LISTS MP_CORE_HEADERS MP_NANOPB_HEADERS MP_CPP_HEADERS)
    get_filename_component(_name "${_hdr}" NAME)
    file(CREATE_LINK "${_hdr}" "${MP_VIRTUAL_INC}/${_name}" SYMBOLIC)
endforeach()

add_subdirectory(src/core)

if(BUILD_EMBEDDED)
    add_subdirectory(src/embedded)
endif()

if(BUILD_CPP)
    add_subdirectory(src/cpp)
endif()

if(BUILD_TOOLS)
    add_subdirectory(src/tools)
endif()
