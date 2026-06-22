# 生成的 proto 文件
set(TEST_PROTO_SRC "${CUSTOMDATA_GENERATED_DIR}/c/masterpilot/proto/test/test.pb.c")

add_executable(test-embedded-roundtrip
    test_decode_roundtrip.c
    ${TEST_PROTO_SRC}
)

target_include_directories(test-embedded-roundtrip PRIVATE
    "$<BUILD_INTERFACE:${CUSTOMDATA_GENERATED_DIR}/c>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>"
)

target_link_libraries(test-embedded-roundtrip
    PRIVATE
        mp::customdata::nanopb
)

target_compile_features(test-embedded-roundtrip PRIVATE c_std_99)