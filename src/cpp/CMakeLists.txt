find_package(Protobuf REQUIRED)

add_library(mp-customdata-protobuf STATIC
    customdata-protobuf-sender.cpp
)
add_library(mp::customdata::protobuf ALIAS mp-customdata-protobuf)
target_include_directories(mp-customdata-protobuf
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/core>"
        "$<INSTALL_INTERFACE:include/masterpilot>"
)
target_link_libraries(mp-customdata-protobuf
    PUBLIC
        mp::customdata::core
        protobuf::libprotobuf
)
target_compile_features(mp-customdata-protobuf PRIVATE cxx_std_20)

install(TARGETS mp-customdata-protobuf
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(FILES customdata-protobuf-sender.hpp DESTINATION include/masterpilot)
