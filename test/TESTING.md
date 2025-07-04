# Meshtastic Time Window Testing Framework

## Overview

This comprehensive testing framework is designed for the Meshtastic Time Window feature, providing automated testing, analysis, and reporting capabilities.

## Components

### Core Testing Components

1. **Test Framework (`meshtastic_test.h`)**
   - Unified test management system
   - Test context tracking
   - Result collection
   - Automated reporting

2. **Common Utilities (`test_common.h`)**
   - Shared functionality
   - Time handling
   - Memory tracking
   - String utilities

3. **Logger (`test_logger.h`)**
   - Multi-level logging
   - File and console output
   - Log rotation
   - Buffer management

### Analysis Components

1. **Metrics Collection (`test_metrics.h`)**
   - Performance metrics
   - Resource usage tracking
   - Test statistics
   - Custom measurements

2. **Log Analysis (`test_log_analyzer.h`)**
   - Pattern detection
   - Statistical analysis
   - Trend identification
   - Issue detection

3. **Anomaly Detection (`test_log_anomaly.h`)**
   - Performance anomalies
   - Error patterns
   - Resource usage spikes
   - Correlation analysis

4. **Log Correlation (`test_log_correlation.h`)**
   - Event relationships
   - Causality chains
   - Pattern matching
   - Dependency analysis

### Visualization and Reporting

1. **Metrics Visualization (`test_metrics_visualization.h`)**
   - Performance graphs
   - Resource usage charts
   - Pattern visualization
   - Interactive dashboards

2. **Report Generation (`test_metrics_export.h`)**
   - HTML reports
   - JSON data export
   - CSV exports
   - SVG charts

### Test Implementation

1. **Time Window Tests (`test_time_window.cpp`)**
   - Basic operations
   - State transitions
   - Error handling
   - Performance testing

2. **Example Tests (`test_example.cpp`)**
   - Framework usage examples
   - Pattern demonstrations
   - Error handling examples
   - Performance tests

### Configuration and Setup

1. **Test Configuration (`test_config.json`)**
   - Framework settings
   - Test parameters
   - Analysis configuration
   - Report settings

2. **Environment Setup (`setup_environment.py`)**
   - Development environment
   - Dependencies
   - Virtual environment
   - Configuration management

3. **Requirements (`requirements.txt`)**
   - Python dependencies
   - Analysis tools
   - Visualization libraries
   - Development utilities

### Automation

1. **CI/CD Integration (`test.yml`)**
   - GitHub Actions workflow
   - Multi-OS testing
   - Automated analysis
   - Report generation

2. **Trend Analysis (`analyze_trends.py`)**
   - Performance tracking
   - Resource usage analysis
   - Success rate monitoring
   - Trend visualization

## Usage

### Setting Up

1. Install dependencies:
```bash
python setup_environment.py --action setup
```

2. Verify installation:
```bash
python setup_environment.py --action validate
```

### Running Tests

1. Run all tests:
```bash
cmake --build . --target test_all
```

2. Run specific categories:
```bash
cmake --build . --target test_unit
cmake --build . --target test_integration
cmake --build . --target test_performance
```

3. Generate reports:
```bash
cmake --build . --target generate_test_report
```

### Analyzing Results

1. View test results:
```bash
python analyze_trends.py --report-dir build/test_output/reports
```

2. Check performance trends:
```bash
python analyze_trends.py --report-dir build/test_output/reports --output-dir analysis
```

## Directory Structure

```
test/
├── CMakeLists.txt           # Build configuration
├── meshtastic_test.h        # Main framework header
├── test_common.h            # Common utilities
├── test_logger.h            # Logging system
├── test_metrics.h           # Metrics collection
├── test_log_analyzer.h      # Log analysis
├── test_log_anomaly.h       # Anomaly detection
├── test_log_correlation.h   # Correlation analysis
├── test_metrics_export.h    # Report generation
├── test_time_window.cpp     # Time Window tests
├── test_example.cpp         # Example tests
├── setup_environment.py     # Environment setup
├── analyze_trends.py        # Trend analysis
├── requirements.txt         # Dependencies
└── test_config.json        # Configuration
```

## Best Practices

1. **Test Organization**
   - Group related tests
   - Use meaningful names
   - Follow naming conventions
   - Maintain independence

2. **Test Implementation**
   - Handle errors properly
   - Clean up resources
   - Use appropriate assertions
   - Document edge cases

3. **Performance Testing**
   - Define baselines
   - Monitor resources
   - Track trends
   - Document conditions

4. **Analysis and Reporting**
   - Review metrics
   - Investigate anomalies
   - Track patterns
   - Document findings

## Contributing

1. **Adding Tests**
   - Follow existing patterns
   - Add documentation
   - Include metrics
   - Test edge cases

2. **Framework Extensions**
   - Follow design patterns
   - Maintain compatibility
   - Add documentation
   - Include examples

## Troubleshooting

1. **Test Failures**
   - Check logs
   - Review metrics
   - Analyze patterns
   - Verify setup

2. **Performance Issues**
   - Monitor resources
   - Check timing
   - Analyze trends
   - Review configurations

3. **Environment Problems**
   - Verify setup
   - Check dependencies
   - Review logs
   - Update configuration

## License

Same as the main Meshtastic project.