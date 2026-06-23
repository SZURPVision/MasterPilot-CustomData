find_package(Protobuf REQUIRED)

file(GLOB PROTO_CPP_SOURCES
    "${CUSTOMDATA_GENERATED_DIR}/cpp/masterpilot/proto/*.pb.cc"
    "${CUSTOMDATA_GENERATED_DIR}/cpp/masterpilot/proto/test/*.pb.cc"
)

add_library(mp-customdata-cpp
    STATIC
    customdata-protobuf-tx.cpp
    customdata-protobuf-rx.cpp
    ${PROTO_CPP_SOURCES}
)
add_library(mp::customdata::cpp ALIAS mp-customdata-cpp)
target_include_directories(mp-customdata-cpp
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>"
        "$<BUILD_INTERFACE:${CUSTOMDATA_GENERATED_DIR}/cpp>"
        "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(mp-customdata-cpp
    PUBLIC
        protobuf::libprotobuf
    PRIVATE
        mp::customdata::core
)
target_compile_features(mp-customdata-cpp PRIVATE cxx_std_20)

install(TARGETS mp-customdata-cpp
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(FILES
    customdata-protobuf-tx.hpp
    customdata-protobuf-rx.hpp
    DESTINATION include/masterpilot)

# protobuf generated
install(
    DIRECTORY "${CUSTOMDATA_GENERATED_DIR}/cpp/masterpilot/proto/"
    DESTINATION include/masterpilot/proto
    FILES_MATCHING PATTERN "*.pb.h"
)

# ── CMake package config ──────────────────────────────────
include(CMakePackageConfigHelpers)

set(CPP_CMAKE_CONFIG_DIR "lib/cmake/customdata-cpp")

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/customdata-cpp-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/customdata-cpp-config.cmake"
    INSTALL_DESTINATION "${CPP_CMAKE_CONFIG_DIR}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/customdata-cpp-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/customdata-cpp-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/customdata-cpp-config-version.cmake"
    DESTINATION "${CPP_CMAKE_CONFIG_DIR}"
)

# ── pkg-config ─────────────────────────────────────────────
set(PKG_CONFIG_REQUIRES "protobuf")
set(PKG_CONFIG_LIBS "-lmp-customdata-cpp")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/customdata-cpp.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/customdata-cpp.pc"
    @ONLY
)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/customdata-cpp.pc"
    DESTINATION "lib/pkgconfig"
)
