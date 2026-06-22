find_package(Protobuf REQUIRED)
find_package(absl REQUIRED)

set(TEST_PROTO_SRC "${CUSTOMDATA_GENERATED_DIR}/cpp/masterpilot/proto/test/test.pb.cc")

add_executable(test-cpp-roundtrip
    test_roundtrip.cpp
    ${TEST_PROTO_SRC}
)

target_include_directories(test-cpp-roundtrip PRIVATE
    "$<BUILD_INTERFACE:${CUSTOMDATA_GENERATED_DIR}/cpp>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>"
)

target_link_libraries(test-cpp-roundtrip PRIVATE
    mp::customdata::cpp
    protobuf::libprotobuf
    absl::log_internal_check_op
)

target_compile_features(test-cpp-roundtrip PRIVATE cxx_std_20)