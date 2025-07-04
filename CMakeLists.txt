# Minimum CMake version
cmake_minimum_required(VERSION 3.12)

# Project name
project(meshtastic-time-window)

# Add protobuf compilation
find_package(Protobuf REQUIRED)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS
    src/mesh/generated/meshtastic/time_window.proto
    src/mesh/generated/meshtastic/portnums.proto
    src/mesh/generated/meshtastic/lora_config.proto
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mesh
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugins
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROTOBUF_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    src/plugins/TimeWindowPlugin.cpp
    src/cli/TimeWindowCLI.cpp
    src/cli/command_time_window.cpp
    src/mesh/RadioInterfaceTime.cpp
    ${PROTO_SRCS}
)

# Create library
add_library(time_window STATIC ${SOURCES})

# Link dependencies
target_link_libraries(time_window
    ${PROTOBUF_LIBRARIES}
    meshtastic_core
)

# Installation
install(TARGETS time_window
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES
    src/plugins/TimeWindowPlugin.h
    src/cli/TimeWindowCLI.h
    src/cli/command_time_window.h
    DESTINATION include/meshtastic/time_window
)

# Testing
enable_testing()
add_executable(test_time_window test/test_radio_time_window.cpp)
target_link_libraries(test_time_window time_window gtest gtest_main)
add_test(NAME test_time_window COMMAND test_time_window)