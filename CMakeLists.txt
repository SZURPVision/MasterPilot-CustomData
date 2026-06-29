cmake_minimum_required(VERSION 3.16)
project(masterpilot-customdata-embedded LANGUAGES C)

file(GLOB_RECURSE EMBEDDED_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

add_library(masterpilot-customdata-embedded STATIC ${EMBEDDED_SOURCES})

target_include_directories(masterpilot-customdata-embedded
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_features(masterpilot-customdata-embedded PRIVATE c_std_99)
target_compile_definitions(masterpilot-customdata-embedded PRIVATE PB_C99_STATIC_ASSERT)
