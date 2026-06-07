include(FindNanopb)

add_library(mp-customdata-nanopb STATIC
    customdata-nanopb-encode.c
    customdata-nanopb-decode.c
)
add_library(mp::customdata::nanopb ALIAS mp-customdata-nanopb)
target_include_directories(mp-customdata-nanopb
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(mp-customdata-nanopb
    PUBLIC
        mp::customdata::core
        nanopb::nanopb
)
target_compile_features(mp-customdata-nanopb PRIVATE c_std_99)

install(TARGETS mp-customdata-nanopb
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(FILES customdata-nanopb.h DESTINATION include/masterpilot)
