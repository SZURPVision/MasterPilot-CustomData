option(BUILD_CORE_SHARED "Build customdata-core as shared library" OFF)

if(BUILD_CORE_SHARED)
    add_library(mp-customdata-core SHARED
        customdata-core.c
    )
    set_target_properties(mp-customdata-core PROPERTIES
        OUTPUT_NAME customdata-core
        VERSION ${PROJECT_VERSION}
        SOVERSION 0
    )
else()
    add_library(mp-customdata-core STATIC
        customdata-core.c
    )
endif()

add_library(mp::customdata::core ALIAS mp-customdata-core)
target_include_directories(mp-customdata-core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
)
target_compile_features(mp-customdata-core PRIVATE c_std_99)

install(TARGETS mp-customdata-core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(FILES customdata-core.h DESTINATION include/masterpilot)
