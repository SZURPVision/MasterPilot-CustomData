# ── C 测试可执行文件 ──────────────────────────────────
# 编译模式：
#   1. Nix 模式：CUSTOMDATA_SRC_DIR 指向 customdata-src 包输出，nanopb 通过 find_package 发现
#   2. 纯 CMake 模式（无 Nix）：需手动设置 CUSTOMDATA_SRC_DIR 到 buf generate 的输出目录

# 源码集合
file(GLOB MPC_C_SRCS "${MPC_SRC_DIR}/*.c")
file(GLOB MPC_GENERATED_C_SRCS "${CUSTOMDATA_SRC_DIR}/c/*.c")

add_executable(mp-customdata-test
    main.c
    ${MPC_C_SRCS}
    ${MPC_GENERATED_C_SRCS}
)

# ── 头文件搜索路径 ──
target_include_directories(mp-customdata-test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${MPC_SRC_DIR}
        ${CUSTOMDATA_SRC_DIR}/c
)

# ── 依赖 nanopb（仅头文件） ──
find_package(Nanopb REQUIRED)
target_link_libraries(mp-customdata-test PRIVATE nanopb::nanopb)

# ── 依赖 Protobuf ──
find_package(Protobuf REQUIRED)
target_link_libraries(mp-customdata-test PRIVATE protobuf::libprotobuf)

# ── C99（兼容 Keil 嵌入式环境） ──
target_compile_features(mp-customdata-test PRIVATE c_std_99)

# ── 测试注册 ──
add_test(NAME mp-customdata-test COMMAND mp-customdata-test)

# ── compile_commands.json（用于 IDE） ──
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)