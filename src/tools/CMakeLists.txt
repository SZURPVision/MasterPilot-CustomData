add_executable(mp-customdata-tools
    main.c
    send.c
    recv.c
    session.c
)
add_executable(mp::customdata::tools ALIAS mp-customdata-tools)
target_link_libraries(mp-customdata-tools
    PRIVATE
        mp::customdata::core
)
target_include_directories(mp-customdata-tools
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:include/masterpilot>"
)
target_compile_features(mp-customdata-tools PRIVATE c_std_99)

install(TARGETS mp-customdata-tools RUNTIME DESTINATION bin)