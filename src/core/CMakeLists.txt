add_library(mp-customdata-core STATIC
    customdata-core.c
)
add_library(mp::customdata::core ALIAS mp-customdata-core)
target_include_directories(mp-customdata-core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:include/masterpilot>"
)
target_compile_features(mp-customdata-core PRIVATE c_std_99)

install(TARGETS mp-customdata-core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(FILES customdata-core.h DESTINATION include/masterpilot)
