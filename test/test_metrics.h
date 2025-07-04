#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <map>
#include <set>
#include <mutex>
#include "test_utils.h"

namespace meshtastic {
namespace test {

/**
 * Performance and test metrics collection
 */
class TestMetrics {
public:
    enum class Category {
        UNIT_TEST,
        INTEGRATION_TEST,
        PERFORMANCE_TEST,
        STRESS_TEST,
        MEMORY_TEST,
        REGRESSION_TEST,
        FUNCTIONAL_TEST,
        SYSTEM_TEST
    };

    struct PerformanceMetric {
        std::string name;
        double value;
        std::string unit;
        double threshold;
        bool passed;
        std::string description;
    };

    struct TestCategory {
        Category category;
        std::vector<std::string> tests;
        std::chrono::milliseconds totalDuration{0};
        size_t totalMemory{0};
        size_t passedCount{0};
        size_t failedCount{0};
        std::vector<PerformanceMetric> metrics;
    };

    static TestMetrics& instance() {
        static TestMetrics metrics;
        return metrics;
    }

    void categorizeTest(const std::string& testName, Category category) {
        const std::lock_guard<std::mutex> lock(mutex_);
        categories_[category].tests.push_back(testName);
    }

    void recordMetric(Category category,
                     const std::string& name,
                     double value,
                     const std::string& unit,
                     double threshold,
                     const std::string& description = "") {
        const std::lock_guard<std::mutex> lock(mutex_);
        categories_[category].metrics.push_back({
            name,
            value,
            unit,
            threshold,
            value <= threshold,
            description
        });
    }

    void updateCategoryStats(Category category,
                           const TestUtils::TestResult& result) {
        const std::lock_guard<std::mutex> lock(mutex_);
        auto& cat = categories_[category];
        cat.totalDuration += result.duration;
        cat.totalMemory += result.memoryUsage;
        if (result.passed) {
            cat.passedCount++;
        } else {
            cat.failedCount++;
        }
    }

    std::string generateMetricsReport() const {
        std::stringstream report;
        report << "Performance and Test Metrics Report\n"
               << "===================================\n\n";

        for (const auto& [category, data] : categories_) {
            report << getCategoryName(category) << "\n"
                   << std::string(getCategoryName(category).length(), '-') << "\n\n"
                   << "Tests: " << data.tests.size() << "\n"
                   << "Passed: " << data.passedCount << "\n"
                   << "Failed: " << data.failedCount << "\n"
                   << "Total Duration: " << formatDuration(data.totalDuration) << "\n"
                   << "Total Memory: " << formatMemory(data.totalMemory) << "\n\n";

            if (!data.metrics.empty()) {
                report << "Performance Metrics:\n";
                for (const auto& metric : data.metrics) {
                    report << "- " << metric.name << ": "
                           << std::fixed << std::setprecision(2) << metric.value
                           << " " << metric.unit
                           << " (Threshold: " << metric.threshold << ")"
                           << " [" << (metric.passed ? "PASS" : "FAIL") << "]\n";
                    if (!metric.description.empty()) {
                        report << "  " << metric.description << "\n";
                    }
                }
                report << "\n";
            }
        }

        return report.str();
    }

    std::string generateJsonMetrics() const {
        std::stringstream json;
        json << "{\n  \"categories\": {\n";
        
        bool firstCategory = true;
        for (const auto& [category, data] : categories_) {
            if (!firstCategory) json << ",\n";
            firstCategory = false;
            
            json << "    \"" << getCategoryName(category) << "\": {\n"
                 << "      \"tests\": " << data.tests.size() << ",\n"
                 << "      \"passed\": " << data.passedCount << ",\n"
                 << "      \"failed\": " << data.failedCount << ",\n"
                 << "      \"duration_ms\": " << data.totalDuration.count() << ",\n"
                 << "      \"memory_bytes\": " << data.totalMemory << ",\n"
                 << "      \"metrics\": [\n";

            bool firstMetric = true;
            for (const auto& metric : data.metrics) {
                if (!firstMetric) json << ",\n";
                firstMetric = false;
                
                json << "        {\n"
                     << "          \"name\": \"" << metric.name << "\",\n"
                     << "          \"value\": " << metric.value << ",\n"
                     << "          \"unit\": \"" << metric.unit << "\",\n"
                     << "          \"threshold\": " << metric.threshold << ",\n"
                     << "          \"passed\": " << (metric.passed ? "true" : "false")
                     << "\n        }";
            }
            
            if (!data.metrics.empty()) json << "\n";
            json << "      ]\n    }";
        }
        
        json << "\n  }\n}";
        return json.str();
    }

    bool checkPerformanceThresholds() const {
        bool allPassed = true;
        for (const auto& [category, data] : categories_) {
            for (const auto& metric : data.metrics) {
                if (!metric.passed) {
                    allPassed = false;
                    std::cerr << "Performance threshold exceeded: "
                             << metric.name << " (" << metric.value
                             << " " << metric.unit << " > "
                             << metric.threshold << ")\n";
                }
            }
        }
        return allPassed;
    }

private:
    std::mutex mutex_;
    std::map<Category, TestCategory> categories_;

    std::string getCategoryName(Category category) const {
        switch (category) {
            case Category::UNIT_TEST: return "Unit Tests";
            case Category::INTEGRATION_TEST: return "Integration Tests";
            case Category::PERFORMANCE_TEST: return "Performance Tests";
            case Category::STRESS_TEST: return "Stress Tests";
            case Category::MEMORY_TEST: return "Memory Tests";
            case Category::REGRESSION_TEST: return "Regression Tests";
            case Category::FUNCTIONAL_TEST: return "Functional Tests";
            case Category::SYSTEM_TEST: return "System Tests";
            default: return "Unknown";
        }
    }

    std::string formatDuration(std::chrono::milliseconds duration) const {
        auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
        duration -= hours;
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
        duration -= minutes;
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
        duration -= seconds;
        
        std::stringstream ss;
        if (hours.count() > 0) {
            ss << hours.count() << "h ";
        }
        if (minutes.count() > 0 || hours.count() > 0) {
            ss << minutes.count() << "m ";
        }
        ss << seconds.count() << "." 
           << std::setw(3) << std::setfill('0') << duration.count() << "s";
        return ss.str();
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
};

} // namespace test
} // namespace meshtastic

// Helper macros for metrics
#define CATEGORIZE_TEST(name, category) \
    meshtastic::test::TestMetrics::instance().categorizeTest(name, category)

#define RECORD_METRIC(category, name, value, unit, threshold, desc) \
    meshtastic::test::TestMetrics::instance().recordMetric(category, name, \
        value, unit, threshold, desc)

#define UPDATE_CATEGORY_STATS(category, result) \
    meshtastic::test::TestMetrics::instance().updateCategoryStats(category, result)

#define GENERATE_METRICS_REPORT() \
    meshtastic::test::TestMetrics::instance().generateMetricsReport()

#define GENERATE_JSON_METRICS() \
    meshtastic::test::TestMetrics::instance().generateJsonMetrics()

#define CHECK_PERFORMANCE_THRESHOLDS() \
    meshtastic::test::TestMetrics::instance().checkPerformanceThresholds()