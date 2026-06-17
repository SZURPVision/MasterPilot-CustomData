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

install(TARGETS mp-customdata-embedded
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(
    FILES
        customdata-embedded-encode.h
        customdata-embedded-encode.h
    DESTINATION include/masterpilot
)
