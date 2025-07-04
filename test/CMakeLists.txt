cmake_minimum_required(VERSION 3.10)
project(MeshtasticTests CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(GTest REQUIRED)
include(GoogleTest)

# Set compile options
add_compile_options(-Wall -Wextra -Werror)

# Define test sources
set(TEST_SOURCES
    test_example.cpp
    test_radio_time_window.cpp
)

# Define test headers
set(TEST_HEADERS
    test_common.h
    test_logger.h
    test_metrics.h
    test_metrics_visualization.h
    test_metrics_export.h
    test_log_analyzer.h
    test_log_anomaly.h
    test_log_correlation.h
    meshtastic_test.h
)

# Create test executable
add_executable(meshtastic_tests ${TEST_SOURCES} ${TEST_HEADERS})

# Include directories
target_include_directories(meshtastic_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${GTEST_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(meshtastic_tests PRIVATE
    GTest::GTest
    GTest::Main
    pthread
)

# Enable testing
enable_testing()
gtest_discover_tests(meshtastic_tests)

# Create test data directory
set(TEST_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR}/test_data)
file(MAKE_DIRECTORY ${TEST_DATA_DIR})

# Define test macros
target_compile_definitions(meshtastic_tests PRIVATE
    TEST_DATA_DIR="${TEST_DATA_DIR}"
)

# Add custom targets for different test types
add_custom_target(test_all
    COMMAND ${CMAKE_COMMAND} -E env "GTEST_COLOR=1"
            ${CMAKE_CURRENT_BINARY_DIR}/meshtastic_tests
    DEPENDS meshtastic_tests
)

add_custom_target(test_unit
    COMMAND ${CMAKE_COMMAND} -E env "GTEST_COLOR=1"
            ${CMAKE_CURRENT_BINARY_DIR}/meshtastic_tests
            --gtest_filter="*Unit*"
    DEPENDS meshtastic_tests
)

add_custom_target(test_integration
    COMMAND ${CMAKE_COMMAND} -E env "GTEST_COLOR=1"
            ${CMAKE_CURRENT_BINARY_DIR}/meshtastic_tests
            --gtest_filter="*Integration*"
    DEPENDS meshtastic_tests
)

add_custom_target(test_performance
    COMMAND ${CMAKE_COMMAND} -E env "GTEST_COLOR=1"
            ${CMAKE_CURRENT_BINARY_DIR}/meshtastic_tests
            --gtest_filter="*Performance*"
    DEPENDS meshtastic_tests
)

# Test report targets
add_custom_target(generate_test_report
    COMMAND ${CMAKE_COMMAND} -E env "GENERATE_REPORTS=1"
            ${CMAKE_CURRENT_BINARY_DIR}/meshtastic_tests
    DEPENDS meshtastic_tests
)

# Clean test data
add_custom_target(clean_test_data
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${TEST_DATA_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_DATA_DIR}
)

# Install targets
install(TARGETS meshtastic_tests
    RUNTIME DESTINATION bin
)

# Test configuration
set_tests_properties(${ALL_TESTS} PROPERTIES
    ENVIRONMENT "TEST_DATA_DIR=${TEST_DATA_DIR}"
)

# Optional: Add coverage support
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
if(ENABLE_COVERAGE)
    target_compile_options(meshtastic_tests PRIVATE --coverage)
    target_link_libraries(meshtastic_tests PRIVATE --coverage)
    
    add_custom_target(coverage
        COMMAND lcov --capture --directory . --output-file coverage.info
        COMMAND lcov --remove coverage.info '/usr/*' --output-file coverage.info
        COMMAND genhtml coverage.info --output-directory coverage_report
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS test_all
    )
endif()

# Output configuration summary
message(STATUS "Test Configuration:")
message(STATUS "  Test Data Directory: ${TEST_DATA_DIR}")
message(STATUS "  Coverage Enabled: ${ENABLE_COVERAGE}")
message(STATUS "  GTest Include Dir: ${GTEST_INCLUDE_DIRS}")

# Add test categories
add_custom_target(test_categories
    COMMAND ${CMAKE_COMMAND} -E echo "Available test categories:"
    COMMAND ${CMAKE_COMMAND} -E echo "  test_all        - Run all tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  test_unit       - Run unit tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  test_integration - Run integration tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  test_performance - Run performance tests"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Additional targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  generate_test_report - Generate detailed test reports"
    COMMAND ${CMAKE_COMMAND} -E echo "  clean_test_data     - Clean test data directory"
    COMMAND ${CMAKE_COMMAND} -E echo "  coverage            - Generate coverage report (if enabled)"
)

# Add documentation
add_custom_target(test_docs
    COMMAND ${CMAKE_COMMAND} -E echo "Generating test documentation..."
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/docs
    # Add documentation generation commands here
)

message(STATUS "Test system configured successfully")