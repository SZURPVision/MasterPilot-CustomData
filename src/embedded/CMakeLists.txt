find_package(nanopb REQUIRED)

add_library(mp-customdata-embedded STATIC
    customdata-embedded-encode.c
    customdata-embedded-decode.c
)
add_library(mp::customdata::nanopb ALIAS mp-customdata-embedded)
target_include_directories(mp-customdata-embedded
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(mp-customdata-embedded
    PUBLIC
        mp::customdata::core
        nanopb::protobuf-nanopb-static
)
target_compile_features(mp-customdata-embedded PRIVATE c_std_99)

if(NOT SRC_DIST)
    install(TARGETS mp-customdata-embedded
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
    )
    install(
        FILES
            customdata-embedded-encode.h
            customdata-embedded-decode.h
        DESTINATION include/masterpilot
    )
endif()

# ── 源码分发 ───────────────────────────────────────────────
if(SRC_DIST)
    get_target_property(_nanopb_inc_dir nanopb::protobuf-nanopb-static
        INTERFACE_INCLUDE_DIRECTORIES)

    # CORE headers (exclude FFI)
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/src/core/"
        DESTINATION include/masterpilot
        FILES_MATCHING PATTERN "*.h"
        PATTERN "*ffi*" EXCLUDE)

    # Embedded headers
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/src/embedded/"
        DESTINATION include/masterpilot
        FILES_MATCHING PATTERN "*.h")

    # Proto headers
    install(DIRECTORY "${CUSTOMDATA_GENERATED_DIR}/c/masterpilot/proto/"
        DESTINATION include/masterpilot/proto
        FILES_MATCHING PATTERN "*.h")

    # Nanopb headers (top-level only)
    file(GLOB _nanopb_hdrs "${_nanopb_inc_dir}/*.h")
    install(FILES ${_nanopb_hdrs} DESTINATION include)

    # CORE sources (exclude FFI)
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/src/core/"
        DESTINATION src
        FILES_MATCHING PATTERN "*.c"
        PATTERN "*ffi*" EXCLUDE)

    # Embedded sources
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/src/embedded/"
        DESTINATION src
        FILES_MATCHING PATTERN "*.c")

    # Nanopb sources (top-level only)
    if(DEFINED NANOPB_SRC_DIR)
        file(GLOB _nanopb_srcs "${NANOPB_SRC_DIR}/*.c")
        install(FILES ${_nanopb_srcs} DESTINATION src)
    endif()

    # Proto sources
    install(DIRECTORY "${CUSTOMDATA_GENERATED_DIR}/c/masterpilot/proto/"
        DESTINATION src
        FILES_MATCHING PATTERN "*.c")
endif()
