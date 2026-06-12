option(BUILD_CORE_SHARED "Build customdata-core as shared library" OFF)

set(CORE_SOURCES
    customdata-common.c
    customdata-tx.c
    customdata-rx.c
    customdata-stream-tx.c
    customdata-stream-rx.c
)

if(BUILD_CORE_SHARED)
    add_library(mp-customdata-core SHARED ${CORE_SOURCES})
    set_target_properties(mp-customdata-core PROPERTIES
        OUTPUT_NAME customdata-core
        VERSION ${PROJECT_VERSION}
        SOVERSION 0
    )
else()
    add_library(mp-customdata-core STATIC ${CORE_SOURCES})
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
install(FILES 
    customdata-common.h
    customdata-tx.h
    customdata-rx.h
    customdata-stream-common.h
    customdata-stream-tx.h
    customdata-stream-rx.h
    DESTINATION include/masterpilot
)
