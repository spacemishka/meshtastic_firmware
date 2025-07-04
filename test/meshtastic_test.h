#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>
#include "test_common.h"
#include "test_logger.h"
#include "test_metrics.h"
#include "test_metrics_visualization.h"
#include "test_metrics_export.h"
#include "test_log_analyzer.h"
#include "test_log_anomaly.h"
#include "test_log_correlation.h"

namespace meshtastic {
namespace test {

/**
 * Main test framework interface
 */
class MeshtasticTest {
public:
    struct TestConfig {
        std::string outputDir = "test_output";
        bool enableLogging = true;
        bool enableMetrics = true;
        bool enableVisualization = true;
        bool enableAnalysis = true;
        TestCommon::LogLevel minLogLevel = TestCommon::LogLevel::INFO;
        size_t maxLogSize = 10 * 1024 * 1024;  // 10MB
        bool saveReports = true;
    };

    struct TestContext {
        std::string name;
        std::chrono::system_clock::time_point startTime;
        std::vector<std::string> logs;
        std::vector<TestMetrics::TestResult> results;
        LogAnalyzer::AnalysisResult logAnalysis;
        LogAnomalyDetector::AnomalyResult anomalies;
        LogCorrelation::CorrelationResult correlations;
    };

    static MeshtasticTest& instance() {
        static MeshtasticTest test;
        return test;
    }

    void initialize(const TestConfig& config = TestConfig()) {
        config_ = config;
        setupComponents();
    }

    TestContext& beginTest(const std::string& name) {
        const std::lock_guard<std::mutex> lock(mutex_);
        contexts_[name] = TestContext{
            name,
            std::chrono::system_clock::now()
        };
        LOG_INFO("Starting test: " + name);
        return contexts_[name];
    }

    void endTest(const std::string& name) {
        const std::lock_guard<std::mutex> lock(mutex_);
        auto it = contexts_.find(name);
        if (it == contexts_.end()) return;

        auto& context = it->second;
        auto duration = std::chrono::system_clock::now() - context.startTime;

        // Analyze results
        if (config_.enableAnalysis) {
            analyzeTestResults(context);
        }

        // Generate reports
        if (config_.saveReports) {
            generateTestReports(context);
        }

        LOG_INFO("Test completed: " + name + " (Duration: " + 
                FORMAT_DURATION(std::chrono::duration_cast<std::chrono::milliseconds>(duration)) + ")");
    }

    void recordResult(const std::string& testName,
                     const TestMetrics::TestResult& result) {
        const std::lock_guard<std::mutex> lock(mutex_);
        auto it = contexts_.find(testName);
        if (it != contexts_.end()) {
            it->second.results.push_back(result);
        }
    }

    void recordLog(const std::string& testName,
                  const std::string& message,
                  TestCommon::LogLevel level = TestCommon::LogLevel::INFO) {
        const std::lock_guard<std::mutex> lock(mutex_);
        auto it = contexts_.find(testName);
        if (it != contexts_.end()) {
            it->second.logs.push_back(message);
            if (config_.enableLogging) {
                LOG(level, message);
            }
        }
    }

    std::string generateSummaryReport() const {
        std::stringstream report;
        report << "Meshtastic Test Summary Report\n"
               << "==============================\n\n";

        size_t totalTests = 0;
        size_t passedTests = 0;
        std::chrono::milliseconds totalDuration(0);

        for (const auto& [name, context] : contexts_) {
            size_t passed = std::count_if(
                context.results.begin(),
                context.results.end(),
                [](const TestMetrics::TestResult& r) { return r.passed; }
            );

            totalTests += context.results.size();
            passedTests += passed;
            totalDuration += std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now() - context.startTime);

            report << "Test: " << name << "\n"
                   << "  Results: " << passed << "/" << context.results.size() << " passed\n"
                   << "  Anomalies: " << context.anomalies.count << "\n"
                   << "  Correlation Score: " << std::fixed << std::setprecision(2)
                   << context.correlations.correlationScore << "\n\n";
        }

        double passRate = totalTests > 0 ? 
            (static_cast<double>(passedTests) / totalTests * 100.0) : 0.0;

        report << "Overall Statistics\n"
               << "-----------------\n"
               << "Total Tests: " << totalTests << "\n"
               << "Passed Tests: " << passedTests << "\n"
               << "Pass Rate: " << std::fixed << std::setprecision(1)
               << passRate << "%\n"
               << "Total Duration: " << FORMAT_DURATION(totalDuration) << "\n"
               << "Peak Memory Usage: " << FORMAT_BYTES(GET_CURRENT_MEMORY()) << "\n";

        return report.str();
    }

    const TestContext* getTestContext(const std::string& name) const {
        auto it = contexts_.find(name);
        return it != contexts_.end() ? &it->second : nullptr;
    }

private:
    std::mutex mutex_;
    TestConfig config_;
    std::map<std::string, TestContext> contexts_;

    void setupComponents() {
        // Configure logger
        TestLogger::LogConfig logConfig;
        logConfig.minLevel = config_.minLogLevel;
        logConfig.maxFileSize = config_.maxLogSize;
        logConfig.logDir = config_.outputDir + "/logs";
        CONFIGURE_LOGGER(logConfig);

        // Add default patterns
        ADD_DEFAULT_PATTERNS();
    }

    void analyzeTestResults(TestContext& context) {
        // Analyze logs
        if (!context.logs.empty()) {
            context.logAnalysis = LogAnalyzer::instance().analyze(context.logs);
            context.anomalies = LogAnomalyDetector::instance().detectAnomalies(context.logs);
            context.correlations = LogCorrelation::instance().analyze(context.logs);
        }
    }

    void generateTestReports(const TestContext& context) {
        std::filesystem::path reportDir = config_.outputDir + "/reports/" + context.name;
        std::filesystem::create_directories(reportDir);

        // Generate test report
        std::ofstream testReport(reportDir / "test_report.html");
        testReport << generateTestReport(context);

        // Generate analysis reports if enabled
        if (config_.enableAnalysis && !context.logs.empty()) {
            std::ofstream analysisReport(reportDir / "analysis_report.html");
            analysisReport << LogAnalyzer::instance().generateReport(context.logAnalysis);

            std::ofstream anomalyReport(reportDir / "anomaly_report.html");
            anomalyReport << LogAnomalyDetector::instance().generateReport(context.anomalies);

            std::ofstream correlationReport(reportDir / "correlation_report.html");
            correlationReport << LogCorrelation::instance().generateReport(context.correlations);
        }

        // Generate visualization if enabled
        if (config_.enableVisualization) {
            MetricsVisualization::instance().generateCharts(
                context.results,
                reportDir / "visualizations"
            );
        }
    }

    std::string generateTestReport(const TestContext& context) const {
        std::stringstream report;
        report << "<!DOCTYPE html>\n<html><head>\n"
               << "<title>Test Report: " << context.name << "</title>\n"
               << "<style>" << getReportStyle() << "</style>\n"
               << "</head><body>\n"
               << "<h1>Test Report: " << ESCAPE_XML(context.name) << "</h1>\n"
               << "<div class='summary'>\n"
               << generateTestSummary(context)
               << "</div>\n"
               << "<div class='results'>\n"
               << generateTestResults(context)
               << "</div>\n"
               << "</body></html>";
        return report.str();
    }

    std::string generateTestSummary(const TestContext& context) const {
        size_t passed = std::count_if(
            context.results.begin(),
            context.results.end(),
            [](const TestMetrics::TestResult& r) { return r.passed; }
        );

        auto duration = std::chrono::system_clock::now() - context.startTime;

        std::stringstream summary;
        summary << "<h2>Summary</h2>\n"
                << "<table class='summary-table'>\n"
                << "<tr><td>Status:</td><td class='"
                << (passed == context.results.size() ? "passed" : "failed")
                << "'>" << passed << "/" << context.results.size()
                << " tests passed</td></tr>\n"
                << "<tr><td>Duration:</td><td>"
                << FORMAT_DURATION(std::chrono::duration_cast<std::chrono::milliseconds>(duration))
                << "</td></tr>\n"
                << "<tr><td>Memory Usage:</td><td>"
                << FORMAT_BYTES(GET_CURRENT_MEMORY())
                << "</td></tr>\n"
                << "</table>\n";
        return summary.str();
    }

    std::string generateTestResults(const TestContext& context) const {
        std::stringstream results;
        results << "<h2>Test Results</h2>\n"
                << "<table class='results-table'>\n"
                << "<tr><th>Test</th><th>Status</th><th>Duration</th>"
                << "<th>Memory</th><th>Message</th></tr>\n";

        for (const auto& result : context.results) {
            results << "<tr class='" << (result.passed ? "passed" : "failed") << "'>\n"
                    << "<td>" << ESCAPE_XML(result.name) << "</td>\n"
                    << "<td>" << (result.passed ? "PASS" : "FAIL") << "</td>\n"
                    << "<td>" << FORMAT_DURATION(result.duration) << "</td>\n"
                    << "<td>" << FORMAT_BYTES(result.memoryUsage) << "</td>\n"
                    << "<td>" << ESCAPE_XML(result.message) << "</td>\n"
                    << "</tr>\n";
        }

        results << "</table>";
        return results.str();
    }

    std::string getReportStyle() const {
        return R"(
            body { font-family: Arial, sans-serif; margin: 20px; }
            h1 { color: #2196F3; }
            .summary { background: #f5f5f5; padding: 20px; border-radius: 5px; }
            .summary-table { width: 100%; border-collapse: collapse; }
            .summary-table td { padding: 8px; }
            .results-table { width: 100%; border-collapse: collapse; margin-top: 20px; }
            .results-table th, .results-table td { border: 1px solid #ddd; padding: 8px; }
            .passed { color: green; }
            .failed { color: red; }
        )";
    }
};

} // namespace test
} // namespace meshtastic

// Helper macros for test framework
#define INIT_TEST_FRAMEWORK(config) \
    meshtastic::test::MeshtasticTest::instance().initialize(config)

#define BEGIN_TEST(name) \
    meshtastic::test::MeshtasticTest::instance().beginTest(name)

#define END_TEST(name) \
    meshtastic::test::MeshtasticTest::instance().endTest(name)

#define RECORD_TEST_LOG(test, message, level) \
    meshtastic::test::MeshtasticTest::instance().recordLog(test, message, level)

#define RECORD_TEST_RESULT(test, result) \
    meshtastic::test::MeshtasticTest::instance().recordResult(test, result)

#define GET_TEST_SUMMARY() \
    meshtastic::test::MeshtasticTest::instance().generateSummaryReport()

#define GET_TEST_CONTEXT(name) \
    meshtastic::test::MeshtasticTest::instance().getTestContext(name)