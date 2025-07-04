#include <gtest/gtest.h>
#include "meshtastic_test.h"
#include "test_common.h"
#include <thread>
#include <chrono>

using namespace meshtastic::test;

class ExampleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test framework with custom config
        TestConfig config;
        config.outputDir = "test_output/example";
        config.enableLogging = true;
        config.enableMetrics = true;
        config.enableVisualization = true;
        config.enableAnalysis = true;
        config.minLogLevel = TestCommon::LogLevel::DEBUG;
        
        INIT_TEST_FRAMEWORK(config);
    }

    void TearDown() override {
        // Generate final summary report
        std::cout << GET_TEST_SUMMARY() << std::endl;
    }

    // Helper to simulate work with logging
    void simulateWork(const std::string& testName,
                     const std::string& operation,
                     std::chrono::milliseconds duration) {
        RECORD_TEST_LOG(testName, "Starting " + operation, TestCommon::LogLevel::INFO);
        std::this_thread::sleep_for(duration);
        RECORD_TEST_LOG(testName, "Completed " + operation, TestCommon::LogLevel::INFO);
    }

    // Helper to generate test error
    void simulateError(const std::string& testName,
                      const std::string& error) {
        RECORD_TEST_LOG(testName, "Error: " + error, TestCommon::LogLevel::ERROR);
        throw std::runtime_error(error);
    }
};

TEST_F(ExampleTest, BasicOperations) {
    const std::string TEST_NAME = "BasicOperations";
    auto& context = BEGIN_TEST(TEST_NAME);

    try {
        // Simulate successful operations
        simulateWork(TEST_NAME, "initialization", std::chrono::milliseconds(100));
        simulateWork(TEST_NAME, "processing", std::chrono::milliseconds(200));
        simulateWork(TEST_NAME, "cleanup", std::chrono::milliseconds(100));

        // Record successful result
        TestMetrics::TestResult result{
            "basic_operations",
            true,
            std::chrono::milliseconds(400),
            GET_CURRENT_MEMORY(),
            "Basic operations completed successfully"
        };
        RECORD_TEST_RESULT(TEST_NAME, result);

    } catch (const std::exception& e) {
        TestMetrics::TestResult result{
            "basic_operations",
            false,
            std::chrono::milliseconds(0),
            GET_CURRENT_MEMORY(),
            e.what()
        };
        RECORD_TEST_RESULT(TEST_NAME, result);
        FAIL() << e.what();
    }

    END_TEST(TEST_NAME);
}

TEST_F(ExampleTest, ErrorHandling) {
    const std::string TEST_NAME = "ErrorHandling";
    auto& context = BEGIN_TEST(TEST_NAME);

    try {
        // Simulate some work
        simulateWork(TEST_NAME, "initialization", std::chrono::milliseconds(100));

        // Simulate an error condition
        simulateError(TEST_NAME, "Simulated error condition");

        FAIL() << "Should have thrown an exception";

    } catch (const std::exception& e) {
        // Record error result
        TestMetrics::TestResult result{
            "error_handling",
            true,  // Test is successful because we expected the error
            std::chrono::milliseconds(100),
            GET_CURRENT_MEMORY(),
            "Successfully caught error: " + std::string(e.what())
        };
        RECORD_TEST_RESULT(TEST_NAME, result);
    }

    END_TEST(TEST_NAME);
}

TEST_F(ExampleTest, PatternDetection) {
    const std::string TEST_NAME = "PatternDetection";
    auto& context = BEGIN_TEST(TEST_NAME);

    try {
        // Generate a repeating pattern of operations
        for (int i = 0; i < 3; i++) {
            RECORD_TEST_LOG(TEST_NAME, "Starting iteration " + std::to_string(i), 
                           TestCommon::LogLevel::INFO);
            
            simulateWork(TEST_NAME, "step 1", std::chrono::milliseconds(100));
            simulateWork(TEST_NAME, "step 2", std::chrono::milliseconds(100));
            simulateWork(TEST_NAME, "step 3", std::chrono::milliseconds(100));
            
            RECORD_TEST_LOG(TEST_NAME, "Completed iteration " + std::to_string(i), 
                           TestCommon::LogLevel::INFO);
        }

        // Record successful result
        TestMetrics::TestResult result{
            "pattern_detection",
            true,
            std::chrono::milliseconds(900),
            GET_CURRENT_MEMORY(),
            "Pattern detection test completed"
        };
        RECORD_TEST_RESULT(TEST_NAME, result);

    } catch (const std::exception& e) {
        TestMetrics::TestResult result{
            "pattern_detection",
            false,
            std::chrono::milliseconds(0),
            GET_CURRENT_MEMORY(),
            e.what()
        };
        RECORD_TEST_RESULT(TEST_NAME, result);
        FAIL() << e.what();
    }

    END_TEST(TEST_NAME);
}

TEST_F(ExampleTest, AnomalyDetection) {
    const std::string TEST_NAME = "AnomalyDetection";
    auto& context = BEGIN_TEST(TEST_NAME);

    try {
        // Normal operations
        simulateWork(TEST_NAME, "normal operation", std::chrono::milliseconds(100));
        simulateWork(TEST_NAME, "normal operation", std::chrono::milliseconds(100));
        
        // Introduce an anomaly (longer duration)
        RECORD_TEST_LOG(TEST_NAME, "Starting long operation", TestCommon::LogLevel::WARNING);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        RECORD_TEST_LOG(TEST_NAME, "Completed long operation", TestCommon::LogLevel::WARNING);
        
        // Back to normal
        simulateWork(TEST_NAME, "normal operation", std::chrono::milliseconds(100));
        
        // Introduce error anomaly
        RECORD_TEST_LOG(TEST_NAME, "Error condition detected", TestCommon::LogLevel::ERROR);
        
        // Record successful result
        TestMetrics::TestResult result{
            "anomaly_detection",
            true,
            std::chrono::milliseconds(800),
            GET_CURRENT_MEMORY(),
            "Anomaly detection test completed"
        };
        RECORD_TEST_RESULT(TEST_NAME, result);

    } catch (const std::exception& e) {
        TestMetrics::TestResult result{
            "anomaly_detection",
            false,
            std::chrono::milliseconds(0),
            GET_CURRENT_MEMORY(),
            e.what()
        };
        RECORD_TEST_RESULT(TEST_NAME, result);
        FAIL() << e.what();
    }

    END_TEST(TEST_NAME);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}