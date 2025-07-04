#include <gtest/gtest.h>
#include "meshtastic_test.h"
#include "mesh/TimeWindow.h"
#include <chrono>
#include <thread>

using namespace meshtastic;
using namespace test;

class TimeWindowTest : public ::testing::Test {
protected:
    void SetUp() override {
        TestConfig config;
        config.outputDir = "test_output/time_window";
        config.enableLogging = true;
        config.enableMetrics = true;
        config.enableAnalysis = true;
        INIT_TEST_FRAMEWORK(config);
    }
};

TEST_F(TimeWindowTest, BasicOperations) {
    const std::string TEST_NAME = "TimeWindowBasic";
    auto& context = BEGIN_TEST(TEST_NAME);

    try {
        TimeWindow window;
        
        // Test default state
        RECORD_TEST_LOG(TEST_NAME, "Testing default state", TestCommon::LogLevel::INFO);
        EXPECT_FALSE(window.isEnabled());
        EXPECT_FALSE(window.isActive());

        // Test enabling
        RECORD_TEST_LOG(TEST_NAME, "Testing window enabling", TestCommon::LogLevel::INFO);
        window.enable();
        EXPECT_TRUE(window.isEnabled());

        // Record successful result
        TestMetrics::TestResult result{
            "basic_operations",
            true,
            std::chrono::milliseconds(100),
            GET_CURRENT_MEMORY(),
            "Basic TimeWindow operations completed successfully"
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

TEST_F(TimeWindowTest, TimeOperations) {
    const std::string TEST_NAME = "TimeWindowOperations";
    auto& context = BEGIN_TEST(TEST_NAME);

    try {
        TimeWindow window;
        window.enable();

        // Test setting window times
        RECORD_TEST_LOG(TEST_NAME, "Setting time window parameters", TestCommon::LogLevel::INFO);
        
        auto now = std::chrono::system_clock::now();
        auto start = now + std::chrono::minutes(1);
        auto end = start + std::chrono::minutes(5);

        window.setStartTime(start);
        window.setEndTime(end);

        EXPECT_EQ(window.getStartTime(), start);
        EXPECT_EQ(window.getEndTime(), end);

        // Test window state before start
        RECORD_TEST_LOG(TEST_NAME, "Testing window state before start time", 
                       TestCommon::LogLevel::INFO);
        EXPECT_FALSE(window.isActive());

        // Record successful result
        TestMetrics::TestResult result{
            "time_operations",
            true,
            std::chrono::milliseconds(200),
            GET_CURRENT_MEMORY(),
            "TimeWindow time operations completed successfully"
        };
        RECORD_TEST_RESULT(TEST_NAME, result);

    } catch (const std::exception& e) {
        TestMetrics::TestResult result{
            "time_operations",
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

TEST_F(TimeWindowTest, ActiveStateTransitions) {
    const std::string TEST_NAME = "TimeWindowTransitions";
    auto& context = BEGIN_TEST(TEST_NAME);

    try {
        TimeWindow window;
        window.enable();

        auto now = std::chrono::system_clock::now();
        
        // Set window to start immediately and last for 2 seconds
        RECORD_TEST_LOG(TEST_NAME, "Setting up immediate time window", 
                       TestCommon::LogLevel::INFO);
        window.setStartTime(now);
        window.setEndTime(now + std::chrono::seconds(2));

        // Check initial active state
        RECORD_TEST_LOG(TEST_NAME, "Checking initial active state", 
                       TestCommon::LogLevel::INFO);
        EXPECT_TRUE(window.isActive());

        // Wait for window to expire
        RECORD_TEST_LOG(TEST_NAME, "Waiting for window expiration", 
                       TestCommon::LogLevel::INFO);
        std::this_thread::sleep_for(std::chrono::seconds(3));

        // Check expired state
        RECORD_TEST_LOG(TEST_NAME, "Checking expired state", TestCommon::LogLevel::INFO);
        EXPECT_FALSE(window.isActive());

        // Record successful result
        TestMetrics::TestResult result{
            "state_transitions",
            true,
            std::chrono::milliseconds(3100),
            GET_CURRENT_MEMORY(),
            "TimeWindow state transitions completed successfully"
        };
        RECORD_TEST_RESULT(TEST_NAME, result);

    } catch (const std::exception& e) {
        TestMetrics::TestResult result{
            "state_transitions",
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

TEST_F(TimeWindowTest, InvalidOperations) {
    const std::string TEST_NAME = "TimeWindowInvalid";
    auto& context = BEGIN_TEST(TEST_NAME);

    try {
        TimeWindow window;
        window.enable();

        auto now = std::chrono::system_clock::now();

        // Test setting end time before start time
        RECORD_TEST_LOG(TEST_NAME, "Testing invalid time window setup", 
                       TestCommon::LogLevel::INFO);
        window.setStartTime(now + std::chrono::minutes(10));
        
        bool exceptionCaught = false;
        try {
            window.setEndTime(now);
        } catch (const std::invalid_argument&) {
            exceptionCaught = true;
            RECORD_TEST_LOG(TEST_NAME, "Caught expected invalid argument exception", 
                           TestCommon::LogLevel::INFO);
        }
        EXPECT_TRUE(exceptionCaught);

        // Test zero-duration window
        exceptionCaught = false;
        try {
            window.setStartTime(now);
            window.setEndTime(now);
        } catch (const std::invalid_argument&) {
            exceptionCaught = true;
            RECORD_TEST_LOG(TEST_NAME, "Caught expected zero duration exception", 
                           TestCommon::LogLevel::INFO);
        }
        EXPECT_TRUE(exceptionCaught);

        // Record successful result
        TestMetrics::TestResult result{
            "invalid_operations",
            true,
            std::chrono::milliseconds(100),
            GET_CURRENT_MEMORY(),
            "TimeWindow invalid operations handled correctly"
        };
        RECORD_TEST_RESULT(TEST_NAME, result);

    } catch (const std::exception& e) {
        TestMetrics::TestResult result{
            "invalid_operations",
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

TEST_F(TimeWindowTest, PerformanceStress) {
    const std::string TEST_NAME = "TimeWindowPerformance";
    auto& context = BEGIN_TEST(TEST_NAME);

    try {
        TimeWindow window;
        window.enable();

        auto now = std::chrono::system_clock::now();
        window.setStartTime(now);
        window.setEndTime(now + std::chrono::hours(1));

        // Stress test with rapid state checks
        RECORD_TEST_LOG(TEST_NAME, "Starting rapid state check stress test", 
                       TestCommon::LogLevel::INFO);
        
        const int iterations = 100000;
        auto startTime = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; i++) {
            window.isActive();
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime);
        
        double avgMicros = static_cast<double>(duration.count()) / iterations;
        
        RECORD_TEST_LOG(TEST_NAME, 
            "Completed " + std::to_string(iterations) + " iterations in " + 
            std::to_string(duration.count()) + " microseconds", 
            TestCommon::LogLevel::INFO);
        RECORD_TEST_LOG(TEST_NAME,
            "Average time per check: " + std::to_string(avgMicros) + " microseconds",
            TestCommon::LogLevel::INFO);

        // Verify performance is within acceptable range (< 1 microsecond per check)
        EXPECT_LT(avgMicros, 1.0);

        // Record successful result
        TestMetrics::TestResult result{
            "performance_stress",
            true,
            std::chrono::milliseconds(duration.count() / 1000),
            GET_CURRENT_MEMORY(),
            "TimeWindow performance stress test completed successfully"
        };
        RECORD_TEST_RESULT(TEST_NAME, result);

    } catch (const std::exception& e) {
        TestMetrics::TestResult result{
            "performance_stress",
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