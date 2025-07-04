#pragma once

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <thread>
#include <future>
#include "test_utils.h"
#include "test_metrics.h"
#include "test_metrics_visualization.h"
#include "test_metrics_export.h"

namespace meshtastic {
namespace test {

/**
 * Test execution and orchestration
 */
class TestRunner {
public:
    struct TestCase {
        std::string name;
        std::function<void()> test;
        TestMetrics::Category category;
        size_t timeout_ms = 5000;
        bool parallel = false;
        std::vector<std::string> dependencies;
    };

    struct TestSuite {
        std::string name;
        std::vector<TestCase> tests;
        bool stopOnFailure = false;
        std::function<void()> setup;
        std::function<void()> teardown;
    };

    struct RunConfig {
        bool parallelExecution = true;
        size_t maxThreads = std::thread::hardware_concurrency();
        bool generateReports = true;
        std::string reportDir = "test_reports";
        std::vector<TestMetrics::Category> categories = {
            TestMetrics::Category::UNIT_TEST,
            TestMetrics::Category::INTEGRATION_TEST,
            TestMetrics::Category::PERFORMANCE_TEST,
            TestMetrics::Category::STRESS_TEST
        };
    };

    static TestRunner& instance() {
        static TestRunner runner;
        return runner;
    }

    void registerTest(const TestCase& test) {
        const std::lock_guard<std::mutex> lock(mutex_);
        tests_.push_back(test);
    }

    void registerSuite(const TestSuite& suite) {
        const std::lock_guard<std::mutex> lock(mutex_);
        suites_.push_back(suite);
    }

    bool runAll(const RunConfig& config = RunConfig()) {
        TestUtils::instance().beginTestSuite("All Tests");
        bool success = true;

        try {
            // Run individual tests
            success &= runTests(tests_, config);

            // Run test suites
            for (const auto& suite : suites_) {
                success &= runSuite(suite, config);
            }

            // Generate reports if enabled
            if (config.generateReports) {
                generateReports(config.reportDir);
            }

        } catch (const std::exception& e) {
            std::cerr << "Test execution failed: " << e.what() << std::endl;
            success = false;
        }

        TestUtils::instance().endTestSuite();
        return success;
    }

private:
    std::mutex mutex_;
    std::vector<TestCase> tests_;
    std::vector<TestSuite> suites_;

    bool runTests(const std::vector<TestCase>& tests, const RunConfig& config) {
        std::vector<std::future<bool>> futures;
        std::vector<TestCase> serialTests;

        // Separate parallel and serial tests
        for (const auto& test : tests) {
            if (shouldRunTest(test, config)) {
                if (test.parallel && config.parallelExecution) {
                    futures.push_back(std::async(std::launch::async,
                        [this, &test]() { return runTest(test); }));
                } else {
                    serialTests.push_back(test);
                }
            }
        }

        // Run parallel tests
        bool success = true;
        for (auto& future : futures) {
            success &= future.get();
        }

        // Run serial tests
        for (const auto& test : serialTests) {
            success &= runTest(test);
        }

        return success;
    }

    bool runSuite(const TestSuite& suite, const RunConfig& config) {
        TestUtils::instance().beginTestSuite(suite.name);
        bool success = true;

        try {
            // Run suite setup
            if (suite.setup) {
                suite.setup();
            }

            // Run tests
            success = runTests(suite.tests, config);

            // Run suite teardown
            if (suite.teardown) {
                suite.teardown();
            }

        } catch (const std::exception& e) {
            std::cerr << "Suite execution failed: " << e.what() << std::endl;
            success = false;
        }

        TestUtils::instance().endTestSuite();
        return success;
    }

    bool runTest(const TestCase& test) {
        auto start = std::chrono::steady_clock::now();
        bool passed = true;
        std::string message;
        std::vector<std::string> errors;

        try {
            // Run test with timeout
            auto future = std::async(std::launch::async, test.test);
            if (future.wait_for(std::chrono::milliseconds(test.timeout_ms)) == 
                std::future_status::timeout) {
                throw std::runtime_error("Test timeout");
            }
            future.get();

        } catch (const std::exception& e) {
            passed = false;
            message = e.what();
            errors.push_back(e.what());
        }

        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // Record result
        TestUtils::TestResult result{
            test.name,
            passed,
            duration,
            getCurrentMemoryUsage(),
            message,
            errors
        };

        TestUtils::instance().recordTestResult(result);
        TestMetrics::instance().updateCategoryStats(test.category, result);

        return passed;
    }

    void generateReports(const std::string& reportDir) {
        auto& metrics = TestMetrics::instance();
        auto& viz = MetricsVisualization::instance();

        // Generate reports in different formats
        MetricsExport::ExportConfig config;
        config.outputDir = reportDir;

        config.format = MetricsExport::Format::HTML;
        MetricsExport::instance().exportMetrics(metrics, "test_report", config);

        config.format = MetricsExport::Format::JSON;
        MetricsExport::instance().exportMetrics(metrics, "test_report", config);

        config.format = MetricsExport::Format::CSV;
        MetricsExport::instance().exportMetrics(metrics, "test_report", config);

        // Generate visualization dashboard
        std::ofstream dashboard(reportDir + "/dashboard.html");
        if (dashboard) {
            dashboard << viz.generateMetricsDashboard(metrics);
        }
    }

    bool shouldRunTest(const TestCase& test, const RunConfig& config) {
        return std::find(config.categories.begin(),
                        config.categories.end(),
                        test.category) != config.categories.end();
    }

    size_t getCurrentMemoryUsage() {
        #ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX pmc;
        GetProcessMemoryInfo(GetCurrentProcess(), 
                           (PROCESS_MEMORY_COUNTERS*)&pmc, 
                           sizeof(pmc));
        return pmc.WorkingSetSize;
        #else
        std::ifstream statm("/proc/self/statm");
        size_t size, resident, share, text, lib, data, dt;
        statm >> size >> resident >> share >> text >> lib >> data >> dt;
        return resident * sysconf(_SC_PAGE_SIZE);
        #endif
    }
};

} // namespace test
} // namespace meshtastic

// Helper macros for test registration
#define REGISTER_TEST(name, category, func) \
    meshtastic::test::TestRunner::instance().registerTest({ \
        name, func, category \
    })

#define REGISTER_TEST_SUITE(name, tests) \
    meshtastic::test::TestRunner::instance().registerSuite({ \
        name, tests \
    })

#define RUN_ALL_TESTS() \
    meshtastic::test::TestRunner::instance().runAll()

#define RUN_TESTS_WITH_CONFIG(config) \
    meshtastic::test::TestRunner::instance().runAll(config)