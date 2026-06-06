include(FindNanopb)

add_executable(mp-customdata-tools
    main.c
)
add_executable(mp::customdata::tools ALIAS mp-customdata-tools)
target_link_libraries(mp-customdata-tools
    PRIVATE
        mp::customdata::core
        mp::customdata::nanopb
)
target_compile_features(mp-customdata-tools PRIVATE c_std_99)

install(TARGETS mp-customdata-tools RUNTIME DESTINATION bin)
