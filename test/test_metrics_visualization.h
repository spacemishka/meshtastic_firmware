#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include "test_metrics.h"

namespace meshtastic {
namespace test {

/**
 * Visualization tools for test metrics
 */
class MetricsVisualization {
public:
    struct ChartConfig {
        int width = 80;            // Terminal width
        int height = 15;           // Chart height
        bool showGrid = true;      // Show background grid
        bool showLabels = true;    // Show value labels
        char barChar = '█';        // Character for bars
        char gridChar = '·';       // Character for grid
    };

    static MetricsVisualization& instance() {
        static MetricsVisualization viz;
        return viz;
    }

    std::string generateAsciiHistogram(const std::vector<double>& values,
                                     const std::vector<std::string>& labels,
                                     const std::string& title,
                                     const ChartConfig& config = ChartConfig()) {
        if (values.empty() || values.size() != labels.size()) return "";

        // Find range
        auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
        double minVal = *minIt;
        double maxVal = std::max(*maxIt, 0.1); // Avoid division by zero

        std::stringstream chart;
        
        // Title
        if (!title.empty()) {
            chart << title << "\n"
                 << std::string(title.length(), '=') << "\n\n";
        }

        // Generate chart
        for (int y = config.height - 1; y >= 0; y--) {
            double threshold = maxVal * (static_cast<double>(y) / (config.height - 1));
            
            // Y-axis labels
            if (config.showLabels) {
                chart << std::fixed << std::setprecision(1)
                      << std::setw(8) << threshold << " │";
            }

            // Bars and grid
            for (size_t x = 0; x < values.size(); x++) {
                if (values[x] >= threshold) {
                    chart << config.barChar;
                } else if (config.showGrid && y % 2 == 0) {
                    chart << config.gridChar;
                } else {
                    chart << ' ';
                }
            }
            chart << "\n";
        }

        // X-axis
        if (config.showLabels) {
            chart << "         ";
            for (size_t i = 0; i < config.width - 10; i++) {
                chart << "─";
            }
            chart << "\n         ";
            
            // X-axis labels
            for (size_t i = 0; i < labels.size(); i++) {
                chart << std::setw(1) << labels[i][0];
            }
        }
        
        return chart.str();
    }

    std::string generatePerformanceGraph(const TestMetrics::TestCategory& category,
                                       const ChartConfig& config = ChartConfig()) {
        std::vector<double> values;
        std::vector<std::string> labels;
        
        for (const auto& metric : category.metrics) {
            values.push_back(metric.value);
            labels.push_back(metric.name);
        }

        return generateAsciiHistogram(values, labels, "Performance Metrics", config);
    }

    std::string generateCategoryComparison(const std::map<TestMetrics::Category,
                                         TestMetrics::TestCategory>& categories,
                                         const ChartConfig& config = ChartConfig()) {
        std::vector<double> passRates;
        std::vector<std::string> labels;

        for (const auto& [category, data] : categories) {
            double total = data.passedCount + data.failedCount;
            if (total > 0) {
                double passRate = (data.passedCount * 100.0) / total;
                passRates.push_back(passRate);
                labels.push_back(getCategoryShortName(category));
            }
        }

        return generateAsciiHistogram(passRates, labels, "Test Pass Rates (%)", config);
    }

    std::string generatePerformanceTrend(const std::vector<TestMetrics::PerformanceMetric>& history,
                                       const ChartConfig& config = ChartConfig()) {
        std::vector<double> values;
        std::vector<std::string> labels;
        
        for (size_t i = 0; i < history.size(); i++) {
            values.push_back(history[i].value);
            labels.push_back(std::to_string(i + 1));
        }

        return generateAsciiHistogram(values, labels, "Performance Trend", config);
    }

    std::string generateSparkline(const std::vector<double>& values) {
        if (values.empty()) return "";

        // Sparkline characters for different levels
        static const char* sparkChars = "▁▂▃▄▅▆▇█";
        const int numLevels = 8;

        // Find range
        auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
        double minVal = *minIt;
        double maxVal = std::max(*maxIt, minVal + 0.1); // Avoid division by zero
        double range = maxVal - minVal;

        std::string sparkline;
        for (double value : values) {
            int level = static_cast<int>((value - minVal) * (numLevels - 1) / range);
            level = std::clamp(level, 0, numLevels - 1);
            sparkline += sparkChars[level];
        }

        return sparkline;
    }

    std::string generateMetricsDashboard(const TestMetrics& metrics) {
        std::stringstream dashboard;
        dashboard << "Test Metrics Dashboard\n"
                 << "=====================\n\n";

        // Overall statistics
        size_t totalTests = 0;
        size_t totalPassed = 0;
        for (const auto& [category, data] : metrics.getCategories()) {
            totalTests += data.passedCount + data.failedCount;
            totalPassed += data.passedCount;
        }

        dashboard << "Overall Progress: [" << generateProgressBar(totalPassed, totalTests) << "]\n"
                 << "Total Tests: " << totalTests << "\n"
                 << "Pass Rate: " << std::fixed << std::setprecision(1)
                 << (totalTests > 0 ? (totalPassed * 100.0 / totalTests) : 0.0) << "%\n\n";

        // Category breakdown
        ChartConfig config;
        config.width = 40;
        config.height = 10;
        dashboard << generateCategoryComparison(metrics.getCategories(), config) << "\n\n";

        // Performance metrics summary
        dashboard << "Performance Summary:\n"
                 << "-------------------\n";
        for (const auto& [category, data] : metrics.getCategories()) {
            if (!data.metrics.empty()) {
                dashboard << "\n" << getCategoryName(category) << ":\n";
                for (const auto& metric : data.metrics) {
                    dashboard << "  " << metric.name << ": "
                             << generateMetricIndicator(metric.value, metric.threshold)
                             << " " << std::fixed << std::setprecision(2)
                             << metric.value << " " << metric.unit << "\n";
                }
            }
        }

        return dashboard.str();
    }

private:
    std::string generateProgressBar(size_t completed, size_t total, int width = 30) {
        if (total == 0) return std::string(width, '-');

        int filledWidth = static_cast<int>(completed * width / total);
        return std::string(filledWidth, '=') +
               std::string(width - filledWidth, '-');
    }

    std::string generateMetricIndicator(double value, double threshold) {
        if (value <= threshold * 0.8) return "✓";
        if (value <= threshold) return "!";
        return "✗";
    }

    std::string getCategoryShortName(TestMetrics::Category category) {
        switch (category) {
            case TestMetrics::Category::UNIT_TEST: return "Unit";
            case TestMetrics::Category::INTEGRATION_TEST: return "Int";
            case TestMetrics::Category::PERFORMANCE_TEST: return "Perf";
            case TestMetrics::Category::STRESS_TEST: return "Strs";
            case TestMetrics::Category::MEMORY_TEST: return "Mem";
            case TestMetrics::Category::REGRESSION_TEST: return "Reg";
            case TestMetrics::Category::FUNCTIONAL_TEST: return "Func";
            case TestMetrics::Category::SYSTEM_TEST: return "Sys";
            default: return "Unk";
        }
    }

    std::string getCategoryName(TestMetrics::Category category) {
        switch (category) {
            case TestMetrics::Category::UNIT_TEST: return "Unit Tests";
            case TestMetrics::Category::INTEGRATION_TEST: return "Integration Tests";
            case TestMetrics::Category::PERFORMANCE_TEST: return "Performance Tests";
            case TestMetrics::Category::STRESS_TEST: return "Stress Tests";
            case TestMetrics::Category::MEMORY_TEST: return "Memory Tests";
            case TestMetrics::Category::REGRESSION_TEST: return "Regression Tests";
            case TestMetrics::Category::FUNCTIONAL_TEST: return "Functional Tests";
            case TestMetrics::Category::SYSTEM_TEST: return "System Tests";
            default: return "Unknown Tests";
        }
    }
};

} // namespace test
} // namespace meshtastic

// Helper macros for visualization
#define VISUALIZE_PERFORMANCE(category) \
    meshtastic::test::MetricsVisualization::instance().generatePerformanceGraph(category)

#define VISUALIZE_CATEGORIES() \
    meshtastic::test::MetricsVisualization::instance().generateCategoryComparison( \
        meshtastic::test::TestMetrics::instance().getCategories())

#define GENERATE_METRICS_DASHBOARD() \
    meshtastic::test::MetricsVisualization::instance().generateMetricsDashboard( \
        meshtastic::test::TestMetrics::instance())

#define GENERATE_SPARKLINE(values) \
    meshtastic::test::MetricsVisualization::instance().generateSparkline(values)