#pragma once

#include <string>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <vector>
#include "test_metrics.h"
#include "test_metrics_visualization.h"

namespace meshtastic {
namespace test {

/**
 * Export functionality for test metrics and visualizations
 */
class MetricsExport {
public:
    enum class Format {
        TEXT,
        HTML,
        JSON,
        CSV,
        SVG
    };

    struct ExportConfig {
        Format format = Format::HTML;
        bool includeVisualizations = true;
        bool includeRawData = true;
        std::string outputDir = "metrics_reports";
        std::string theme = "default";
    };

    static MetricsExport& instance() {
        static MetricsExport exporter;
        return exporter;
    }

    bool exportMetrics(const TestMetrics& metrics,
                      const std::string& filename,
                      const ExportConfig& config = ExportConfig()) {
        try {
            std::filesystem::path outputPath = getOutputPath(filename, config);
            std::filesystem::create_directories(outputPath.parent_path());

            switch (config.format) {
                case Format::HTML:
                    return exportHtml(metrics, outputPath, config);
                case Format::JSON:
                    return exportJson(metrics, outputPath, config);
                case Format::CSV:
                    return exportCsv(metrics, outputPath, config);
                case Format::SVG:
                    return exportSvg(metrics, outputPath, config);
                default:
                    return exportText(metrics, outputPath, config);
            }
        } catch (const std::exception& e) {
            lastError_ = e.what();
            return false;
        }
    }

    std::string getLastError() const {
        return lastError_;
    }

private:
    std::string lastError_;

    std::filesystem::path getOutputPath(const std::string& filename,
                                      const ExportConfig& config) const {
        std::filesystem::path base = config.outputDir;
        std::string ext;
        
        switch (config.format) {
            case Format::HTML: ext = ".html"; break;
            case Format::JSON: ext = ".json"; break;
            case Format::CSV: ext = ".csv"; break;
            case Format::SVG: ext = ".svg"; break;
            default: ext = ".txt"; break;
        }

        return base / (filename + ext);
    }

    bool exportHtml(const TestMetrics& metrics,
                   const std::filesystem::path& path,
                   const ExportConfig& config) {
        std::ofstream file(path);
        if (!file) return false;

        // HTML header with styling
        file << "<!DOCTYPE html>\n"
             << "<html><head>\n"
             << "<title>Meshtastic Test Metrics Report</title>\n"
             << "<style>\n"
             << getHtmlStyle(config.theme)
             << "</style>\n"
             << "</head><body>\n"
             << "<div class='container'>\n";

        // Summary section
        file << "<h1>Test Metrics Report</h1>\n"
             << "<div class='summary'>\n"
             << generateSummaryHtml(metrics)
             << "</div>\n";

        // Visualizations
        if (config.includeVisualizations) {
            file << "<div class='visualizations'>\n"
                 << "<h2>Visualizations</h2>\n"
                 << generateVisualizationsHtml(metrics)
                 << "</div>\n";
        }

        // Detailed metrics
        file << "<div class='metrics'>\n"
             << "<h2>Detailed Metrics</h2>\n"
             << generateMetricsHtml(metrics)
             << "</div>\n";

        if (config.includeRawData) {
            file << "<div class='raw-data'>\n"
                 << "<h2>Raw Data</h2>\n"
                 << "<pre>" << generateRawDataJson(metrics) << "</pre>\n"
                 << "</div>\n";
        }

        file << "</div></body></html>\n";
        return true;
    }

    bool exportJson(const TestMetrics& metrics,
                   const std::filesystem::path& path,
                   const ExportConfig& config) {
        std::ofstream file(path);
        if (!file) return false;

        file << "{\n"
             << "  \"timestamp\": \"" << getCurrentTimestamp() << "\",\n"
             << "  \"metrics\": " << generateRawDataJson(metrics);

        if (config.includeVisualizations) {
            file << ",\n  \"visualizations\": {\n"
                 << "    \"charts\": " << generateChartsJson(metrics)
                 << "\n  }";
        }

        file << "\n}\n";
        return true;
    }

    bool exportCsv(const TestMetrics& metrics,
                  const std::filesystem::path& path,
                  const ExportConfig& config) {
        std::ofstream file(path);
        if (!file) return false;

        // Header
        file << "Category,Test,Status,Duration,Memory,Metrics\n";

        // Data rows
        for (const auto& [category, data] : metrics.getCategories()) {
            for (const auto& result : data.results) {
                file << getCategoryName(category) << ","
                     << escapeCsv(result.name) << ","
                     << (result.passed ? "PASS" : "FAIL") << ","
                     << result.duration.count() << ","
                     << result.memoryUsage << ","
                     << generateMetricsCsv(data.metrics)
                     << "\n";
            }
        }

        return true;
    }

    bool exportSvg(const TestMetrics& metrics,
                   const std::filesystem::path& path,
                   const ExportConfig& config) {
        std::ofstream file(path);
        if (!file) return false;

        // SVG header
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
             << "<svg width=\"800\" height=\"600\" xmlns=\"http://www.w3.org/2000/svg\">\n"
             << "<style>\n"
             << getSvgStyle()
             << "</style>\n";

        // Generate chart elements
        file << "<g transform=\"translate(50,50)\">\n"
             << generateChartsSvg(metrics)
             << "</g>\n";

        file << "</svg>\n";
        return true;
    }

    bool exportText(const TestMetrics& metrics,
                   const std::filesystem::path& path,
                   const ExportConfig& config) {
        std::ofstream file(path);
        if (!file) return false;

        auto& viz = MetricsVisualization::instance();
        file << viz.generateMetricsDashboard(metrics);
        return true;
    }

    std::string getCurrentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char buffer[64];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
        return buffer;
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

    std::string getHtmlStyle(const std::string& theme) const {
        if (theme == "dark") {
            return R"(
                body { background: #1e1e1e; color: #e0e0e0; font-family: sans-serif; }
                .container { max-width: 1200px; margin: 0 auto; padding: 20px; }
                h1, h2 { color: #4CAF50; }
                .summary { background: #2d2d2d; padding: 20px; border-radius: 5px; }
                .metrics { margin-top: 20px; }
                pre { background: #2d2d2d; padding: 10px; border-radius: 5px; }
            )";
        }
        return R"(
            body { font-family: sans-serif; line-height: 1.6; margin: 0; padding: 20px; }
            .container { max-width: 1200px; margin: 0 auto; }
            h1, h2 { color: #2196F3; }
            .summary { background: #f5f5f5; padding: 20px; border-radius: 5px; }
            .metrics { margin-top: 20px; }
            pre { background: #f5f5f5; padding: 10px; border-radius: 5px; }
        )";
    }

    std::string getSvgStyle() const {
        return R"(
            .chart-title { font-size: 14px; fill: #333; }
            .axis-label { font-size: 12px; fill: #666; }
            .grid { stroke: #eee; stroke-width: 1; }
            .bar { fill: #2196F3; }
            .bar:hover { fill: #1976D2; }
        )";
    }
};

} // namespace test
} // namespace meshtastic

// Helper macros for metrics export
#define EXPORT_METRICS(metrics, filename, format) \
    meshtastic::test::MetricsExport::instance().exportMetrics(metrics, filename, \
        meshtastic::test::MetricsExport::ExportConfig{format})

#define EXPORT_METRICS_WITH_CONFIG(metrics, filename, config) \
    meshtastic::test::MetricsExport::instance().exportMetrics(metrics, filename, config)