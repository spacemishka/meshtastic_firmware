#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <memory>
#include <iostream>

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "test_data"
#endif

namespace meshtastic {
namespace test {

/**
 * Test utilities for data handling and reporting
 */
class TestUtils {
public:
    struct TestResult {
        std::string name;
        bool passed;
        std::chrono::milliseconds duration;
        size_t memoryUsage;
        std::string message;
        std::vector<std::string> errors;
    };

    struct TestSuiteResult {
        std::string name;
        std::vector<TestResult> results;
        std::chrono::milliseconds totalDuration{0};
        size_t totalMemoryUsage{0};
        size_t passedCount{0};
        size_t failedCount{0};
    };

    static TestUtils& instance() {
        static TestUtils utils;
        return utils;
    }

    // Test data management
    std::string loadTestData(const std::string& filename) {
        const auto dataPath = getTestDataPath() / filename;
        std::ifstream file(dataPath, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Could not load test data: " + filename);
        }
        return std::string(std::istreambuf_iterator<char>(file),
                          std::istreambuf_iterator<char>());
    }

    void saveTestData(const std::string& filename, const std::string& data) {
        const auto dataPath = getTestDataPath() / filename;
        std::filesystem::create_directories(dataPath.parent_path());
        std::ofstream file(dataPath, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Could not save test data: " + filename);
        }
        file.write(data.data(), data.size());
    }

    // Test result recording
    void recordTestResult(const TestResult& result) {
        const std::lock_guard<std::mutex> lock(mutex_);
        currentSuite_.results.push_back(result);
        currentSuite_.totalDuration += result.duration;
        currentSuite_.totalMemoryUsage += result.memoryUsage;
        if (result.passed) {
            currentSuite_.passedCount++;
        } else {
            currentSuite_.failedCount++;
        }
    }

    void beginTestSuite(const std::string& name) {
        const std::lock_guard<std::mutex> lock(mutex_);
        finishCurrentSuite();
        currentSuite_ = TestSuiteResult{name};
    }

    void endTestSuite() {
        const std::lock_guard<std::mutex> lock(mutex_);
        finishCurrentSuite();
    }

    void generateTestReport(const std::string& filename = "test_report.html") {
        const auto reportPath = getReportPath() / filename;
        std::filesystem::create_directories(reportPath.parent_path());
        
        std::ofstream report(reportPath);
        if (report) {
            report << generateHtmlReport();
        }
    }

    void generateCsvReport(const std::string& filename = "test_report.csv") {
        const auto reportPath = getReportPath() / filename;
        std::filesystem::create_directories(reportPath.parent_path());
        
        std::ofstream report(reportPath);
        if (report) {
            report << generateCsvReport();
        }
    }

    void generateJUnitReport(const std::string& filename = "test_report.xml") {
        const auto reportPath = getReportPath() / filename;
        std::filesystem::create_directories(reportPath.parent_path());
        
        std::ofstream report(reportPath);
        if (report) {
            report << generateJUnitXml();
        }
    }

private:
    mutable std::mutex mutex_;
    TestSuiteResult currentSuite_;
    std::vector<TestSuiteResult> completedSuites_;

    std::filesystem::path getTestDataPath() const {
        return std::filesystem::path(TEST_DATA_DIR);
    }

    std::filesystem::path getReportPath() const {
        return std::filesystem::path(TEST_DATA_DIR) / "reports";
    }

    void finishCurrentSuite() {
        if (!currentSuite_.name.empty()) {
            completedSuites_.push_back(currentSuite_);
            currentSuite_ = TestSuiteResult{};
        }
    }

    std::string generateHtmlReport() const {
        std::stringstream html;
        html << "<!DOCTYPE html>\n"
             << "<html><head>\n"
             << "<title>Meshtastic Test Report</title>\n"
             << "<style>\n"
             << "body { font-family: Arial, sans-serif; margin: 20px; }\n"
             << ".suite { margin-bottom: 20px; }\n"
             << ".passed { color: green; }\n"
             << ".failed { color: red; }\n"
             << "table { border-collapse: collapse; width: 100%; }\n"
             << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n"
             << "th { background-color: #f4f4f4; }\n"
             << "</style></head><body>\n"
             << "<h1>Meshtastic Test Report</h1>\n";

        // Summary section
        html << "<h2>Summary</h2>\n"
             << "<table><tr><th>Suite</th><th>Passed</th><th>Failed</th>"
             << "<th>Duration</th><th>Memory</th></tr>\n";

        for (const auto& suite : completedSuites_) {
            html << "<tr><td>" << suite.name << "</td>"
                 << "<td class='passed'>" << suite.passedCount << "</td>"
                 << "<td class='failed'>" << suite.failedCount << "</td>"
                 << "<td>" << suite.totalDuration.count() << "ms</td>"
                 << "<td>" << formatMemory(suite.totalMemoryUsage) << "</td></tr>\n";
        }
        html << "</table>\n";

        // Detailed results
        for (const auto& suite : completedSuites_) {
            html << "<div class='suite'>\n"
                 << "<h3>Suite: " << suite.name << "</h3>\n"
                 << "<table><tr><th>Test</th><th>Status</th><th>Duration</th>"
                 << "<th>Memory</th><th>Message</th></tr>\n";

            for (const auto& result : suite.results) {
                html << "<tr><td>" << result.name << "</td>"
                     << "<td class='" << (result.passed ? "passed" : "failed") << "'>"
                     << (result.passed ? "PASS" : "FAIL") << "</td>"
                     << "<td>" << result.duration.count() << "ms</td>"
                     << "<td>" << formatMemory(result.memoryUsage) << "</td>"
                     << "<td>" << result.message;

                if (!result.errors.empty()) {
                    html << "<ul>";
                    for (const auto& error : result.errors) {
                        html << "<li>" << error << "</li>";
                    }
                    html << "</ul>";
                }
                html << "</td></tr>\n";
            }
            html << "</table></div>\n";
        }

        html << "</body></html>";
        return html.str();
    }

    std::string generateCsvReport() const {
        std::stringstream csv;
        csv << "Suite,Test,Status,Duration (ms),Memory Usage,Message\n";

        for (const auto& suite : completedSuites_) {
            for (const auto& result : suite.results) {
                csv << escapeCsv(suite.name) << ","
                    << escapeCsv(result.name) << ","
                    << (result.passed ? "PASS" : "FAIL") << ","
                    << result.duration.count() << ","
                    << result.memoryUsage << ","
                    << escapeCsv(result.message) << "\n";
            }
        }

        return csv.str();
    }

    std::string generateJUnitXml() const {
        std::stringstream xml;
        xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            << "<testsuites>\n";

        for (const auto& suite : completedSuites_) {
            xml << "  <testsuite name=\"" << escapeXml(suite.name)
                << "\" tests=\"" << suite.results.size()
                << "\" failures=\"" << suite.failedCount
                << "\" time=\"" << (suite.totalDuration.count() / 1000.0)
                << "\">\n";

            for (const auto& result : suite.results) {
                xml << "    <testcase name=\"" << escapeXml(result.name)
                    << "\" time=\"" << (result.duration.count() / 1000.0)
                    << "\"";

                if (!result.passed) {
                    xml << ">\n      <failure message=\""
                        << escapeXml(result.message) << "\">";
                    for (const auto& error : result.errors) {
                        xml << escapeXml(error) << "\n";
                    }
                    xml << "</failure>\n    </testcase>\n";
                } else {
                    xml << "/>\n";
                }
            }
            xml << "  </testsuite>\n";
        }
        xml << "</testsuites>";
        return xml.str();
    }

    std::string formatMemory(size_t bytes) const {
        static const char* units[] = {"B", "KB", "MB", "GB"};
        int unit = 0;
        double size = static_cast<double>(bytes);
        while (size >= 1024.0 && unit < 3) {
            size /= 1024.0;
            unit++;
        }
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << size << " " << units[unit];
        return ss.str();
    }

    std::string escapeCsv(const std::string& str) const {
        if (str.find_first_of(",\"\n") == std::string::npos) {
            return str;
        }
        std::stringstream ss;
        ss << '"';
        for (char c : str) {
            if (c == '"') ss << '"';
            ss << c;
        }
        ss << '"';
        return ss.str();
    }

    std::string escapeXml(const std::string& str) const {
        std::string result;
        result.reserve(str.size());
        for (char c : str) {
            switch (c) {
                case '<': result += "&lt;"; break;
                case '>': result += "&gt;"; break;
                case '&': result += "&amp;"; break;
                case '"': result += "&quot;"; break;
                case '\'': result += "&apos;"; break;
                default: result += c;
            }
        }
        return result;
    }
};

} // namespace test
} // namespace meshtastic

// Helper macros for test utilities
#define LOAD_TEST_DATA(filename) \
    meshtastic::test::TestUtils::instance().loadTestData(filename)

#define SAVE_TEST_DATA(filename, data) \
    meshtastic::test::TestUtils::instance().saveTestData(filename, data)

#define BEGIN_TEST_SUITE(name) \
    meshtastic::test::TestUtils::instance().beginTestSuite(name)

#define END_TEST_SUITE() \
    meshtastic::test::TestUtils::instance().endTestSuite()

#define RECORD_TEST_RESULT(result) \
    meshtastic::test::TestUtils::instance().recordTestResult(result)

#define GENERATE_TEST_REPORTS() \
    do { \
        meshtastic::test::TestUtils::instance().generateTestReport(); \
        meshtastic::test::TestUtils::instance().generateCsvReport(); \
        meshtastic::test::TestUtils::instance().generateJUnitReport(); \
    } while(0)