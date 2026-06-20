find_package(Protobuf REQUIRED)

add_library(mp-customdata-cpp STATIC
    customdata-protobuf-tx.cpp
    customdata-protobuf-rx.cpp
)
add_library(mp::customdata::cpp ALIAS mp-customdata-cpp)
target_include_directories(mp-customdata-cpp
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>"
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
