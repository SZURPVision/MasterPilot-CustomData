# Middlewares/CustomData/CMakeLists.txt

# 暂时只有nanopb配置

# 定义 Nanopb 运行库源码
set(NANOPB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/nanopb)
set(NANOPB_SRCS
    ${NANOPB_DIR}/pb_common.c
    ${NANOPB_DIR}/pb_encode.c
    ${NANOPB_DIR}/pb_decode.c
)

# 定义生成的协议源码
set(PROTO_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated/c)
file(GLOB PROTO_SRCS "${PROTO_GEN_DIR}/*.pb.c")

# 创建静态库
add_library(custom_data STATIC ${NANOPB_SRCS} ${PROTO_SRCS})

# 设置头文件包含路径
target_include_directories(custom_data PUBLIC 
    ${NANOPB_DIR}
    ${PROTO_GEN_DIR}
)

# Nanopb 配置 (如果需要 C99 静态断言)
target_compile_definitions(custom_data PUBLIC PB_C99_STATIC_ASSERT)
