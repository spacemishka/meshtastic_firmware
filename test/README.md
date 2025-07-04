# Meshtastic Testing Framework

This document describes the testing framework for the Meshtastic firmware, specifically focusing on the Time Window feature and related components.

## Overview

The testing framework provides comprehensive tools for:
- Test execution and management
- Logging and metrics collection
- Performance analysis
- Pattern and anomaly detection
- Result reporting and visualization

## Components

### Core Components

1. **Test Runner (`meshtastic_test.h`)**
   - Test execution management
   - Context tracking
   - Result collection
   - Report generation

2. **Common Utilities (`test_common.h`)**
   - Time formatting
   - Memory tracking
   - String utilities
   - Cross-platform support

3. **Logging System (`test_logger.h`)**
   - Multiple log levels
   - File and console output
   - Log rotation
   - Buffer management

### Analysis Components

1. **Metrics Collection (`test_metrics.h`)**
   - Performance metrics
   - Resource usage
   - Test statistics
   - Custom measurements

2. **Log Analysis (`test_log_analyzer.h`)**
   - Pattern detection
   - Trend analysis
   - Statistical analysis
   - Report generation

3. **Anomaly Detection (`test_log_anomaly.h`)**
   - Performance anomalies
   - Error patterns
   - Resource spikes
   - Correlation analysis

### Visualization and Reporting

1. **Metrics Visualization (`test_metrics_visualization.h`)**
   - Performance graphs
   - Resource usage charts
   - Pattern visualization
   - Interactive dashboards

2. **Report Generation (`test_metrics_export.h`)**
   - HTML reports
   - CSV exports
   - JSON data
   - SVG charts

## Usage

### Basic Test Structure

```cpp
class MyTest : public ::testing::Test {
protected:
    void SetUp() override {
        TestConfig config;
        config.outputDir = "test_output/my_test";
        config.enableLogging = true;
        INIT_TEST_FRAMEWORK(config);
    }
};

TEST_F(MyTest, TestCase) {
    const std::string TEST_NAME = "MyTestCase";
    auto& context = BEGIN_TEST(TEST_NAME);

    try {
        // Test implementation
        RECORD_TEST_LOG(TEST_NAME, "Operation started", TestCommon::LogLevel::INFO);
        
        // Record result
        TestMetrics::TestResult result{
            "test_case",
            true,
            std::chrono::milliseconds(100),
            GET_CURRENT_MEMORY(),
            "Test completed successfully"
        };
        RECORD_TEST_RESULT(TEST_NAME, result);

    } catch (const std::exception& e) {
        RECORD_TEST_RESULT(TEST_NAME, {
            "test_case",
            false,
            std::chrono::milliseconds(0),
            GET_CURRENT_MEMORY(),
            e.what()
        });
        FAIL() << e.what();
    }

    END_TEST(TEST_NAME);
}
```

### Running Tests

```bash
# Run all tests
cmake --build . --target test_all

# Run specific test categories
cmake --build . --target test_unit
cmake --build . --target test_integration
cmake --build . --target test_performance

# Generate test reports
cmake --build . --target generate_test_report
```

### Configuration

The test framework can be configured through `TestConfig`:

```cpp
TestConfig config;
config.outputDir = "test_output";            // Output directory
config.enableLogging = true;                 // Enable logging
config.enableMetrics = true;                 // Enable metrics collection
config.enableVisualization = true;           // Enable visualization
config.enableAnalysis = true;                // Enable analysis
config.minLogLevel = TestCommon::LogLevel::DEBUG;  // Minimum log level
```

## Test Categories

1. **Unit Tests**
   - Basic functionality
   - Error handling
   - Edge cases
   - Component isolation

2. **Integration Tests**
   - Component interaction
   - System flow
   - State transitions
   - Error propagation

3. **Performance Tests**
   - Response times
   - Resource usage
   - Stress conditions
   - Scalability limits

4. **System Tests**
   - End-to-end scenarios
   - Real-world conditions
   - Error recovery
   - System stability

## Best Practices

1. **Test Organization**
   - Use meaningful test names
   - Group related tests
   - Maintain test independence
   - Clean up resources

2. **Logging**
   - Use appropriate log levels
   - Include relevant context
   - Log state transitions
   - Record timing information

3. **Error Handling**
   - Test error conditions
   - Verify error messages
   - Check error recovery
   - Validate error states

4. **Performance Testing**
   - Define baselines
   - Measure consistently
   - Monitor resources
   - Document conditions

## Output Analysis

The framework generates several types of reports:

1. **Test Reports**
   - Test results
   - Pass/fail status
   - Execution time
   - Memory usage

2. **Performance Reports**
   - Timing statistics
   - Resource metrics
   - Performance graphs
   - Trend analysis

3. **Analysis Reports**
   - Pattern detection
   - Anomaly findings
   - Correlation analysis
   - System insights

## Extending the Framework

To add new test capabilities:

1. Create new test files
2. Update CMakeLists.txt
3. Add test categories
4. Implement custom metrics

## Troubleshooting

Common issues and solutions:

1. **Test Failures**
   - Check log files
   - Review error messages
   - Verify configurations
   - Check resource usage

2. **Performance Issues**
   - Monitor system resources
   - Review timing logs
   - Check for bottlenecks
   - Analyze patterns

3. **Report Generation**
   - Verify output directory
   - Check file permissions
   - Validate data formats
   - Review error logs

## Contributing

When adding new tests:

1. Follow existing patterns
2. Add documentation
3. Include error handling
4. Add performance metrics
5. Update test reports

## License

Same as the main Meshtastic project