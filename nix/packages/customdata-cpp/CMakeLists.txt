cmake_minimum_required(VERSION 3.16)
project(customdata-cpp VERSION 0.1.0 LANGUAGES CXX)

find_package(protobuf CONFIG REQUIRED)

# Generated proto source files
file(GLOB PROTO_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.pb.cc")
file(GLOB PROTO_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.pb.h")

# Future wrapper sources from src/ directory
file(GLOB WRAPPER_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/wrappers/*.cpp")

# Create static library
add_library(customdata-cpp STATIC ${PROTO_SRCS} ${WRAPPER_SRCS})

# Include paths
target_include_directories(customdata-cpp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Link protobuf
target_link_libraries(customdata-cpp PUBLIC protobuf::libprotobuf)

# C++17
target_compile_features(customdata-cpp PUBLIC cxx_std_17)

# Install
install(TARGETS customdata-cpp
    ARCHIVE DESTINATION lib
)

install(FILES ${PROTO_HDRS} DESTINATION include)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt DESTINATION share/customdata-cpp)
