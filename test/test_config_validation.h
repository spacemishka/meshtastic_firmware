#pragma once

#include <string>
#include <vector>
#include <set>
#include <functional>
#include <sstream>
#include <iomanip>
#include <thread>
#include <filesystem>
#include "test_config.h"

namespace meshtastic {
namespace test {

/**
 * Configuration validation for test system
 */
class ConfigValidator {
public:
    struct ValidationRule {
        std::string name;
        std::function<bool(const TestConfig::VisualizationTestConfig&)> check;
        std::string message;
    };

    struct ValidationResult {
        bool isValid = true;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };

    static ConfigValidator& instance() {
        static ConfigValidator validator;
        return validator;
    }

    void addRule(const std::string& name, 
                std::function<bool(const TestConfig::VisualizationTestConfig&)> check,
                const std::string& message,
                bool isWarning = false) {
        rules.push_back({name, check, message});
        if (isWarning) {
            warnings.insert(name);
        }
    }

    ValidationResult validate(const TestConfig::VisualizationTestConfig& config) {
        ValidationResult result;
        
        // Basic validation rules
        addBasicRules();
        
        // Performance validation rules
        addPerformanceRules();
        
        // Stress test validation rules
        addStressRules();
        
        // Visualization validation rules
        addVisualizationRules();

        // Run all validation rules
        for (const auto& rule : rules) {
            if (!rule.check(config)) {
                if (warnings.count(rule.name)) {
                    result.warnings.push_back(rule.message);
                } else {
                    result.isValid = false;
                    result.errors.push_back(rule.message);
                }
            }
        }

        return result;
    }

    std::string generateReport(const ValidationResult& result) {
        std::ostringstream report;
        report << "Configuration Validation Report\n"
               << "==============================\n\n"
               << "Status: " << (result.isValid ? "Valid" : "Invalid") << "\n\n";

        if (!result.errors.empty()) {
            report << "Errors:\n";
            for (const auto& error : result.errors) {
                report << "- " << error << "\n";
            }
            report << "\n";
        }

        if (!result.warnings.empty()) {
            report << "Warnings:\n";
            for (const auto& warning : result.warnings) {
                report << "- " << warning << "\n";
            }
            report << "\n";
        }

        return report.str();
    }

private:
    std::vector<ValidationRule> rules;
    std::set<std::string> warnings;

    void addBasicRules() {
        // Test data size validation
        addRule("testDataSize", 
            [](const TestConfig::VisualizationTestConfig& config) { 
                return config.testDataSize > 0; 
            },
            "Test data size must be greater than 0");
        
        addRule("testDataSizeWarning",
            [](const TestConfig::VisualizationTestConfig& config) { 
                return config.testDataSize <= 100000; 
            },
            "Large test data size may impact performance",
            true);

        // Iterations validation
        addRule("iterations",
            [](const TestConfig::VisualizationTestConfig& config) { 
                return config.iterations > 0; 
            },
            "Iteration count must be greater than 0");
        
        // Output directory validation
        addRule("outputDir",
            [](const TestConfig::VisualizationTestConfig& config) { 
                return !config.outputDir.empty() && isValidPath(config.outputDir); 
            },
            "Output directory must be valid");
    }

    void addPerformanceRules() {
        // Thread count validation
        addRule("threadCount",
            [](const TestConfig::VisualizationTestConfig& config) { 
                return config.performance.threadCount > 0; 
            },
            "Thread count must be greater than 0");
        
        addRule("threadCountWarning",
            [](const TestConfig::VisualizationTestConfig& config) { 
                return config.performance.threadCount <= std::thread::hardware_concurrency(); 
            },
            "Thread count exceeds hardware concurrency",
            true);

        // Data size range validation
        addRule("dataSizeRange",
            [](const TestConfig::VisualizationTestConfig& config) { 
                return config.performance.maxDataSize > config.performance.minDataSize; 
            },
            "Maximum data size must be greater than minimum data size");
    }

    void addStressRules() {
        // Duration validation
        addRule("duration",
            [](const TestConfig::VisualizationTestConfig& config) { 
                return config.stress.durationMinutes > 0; 
            },
            "Stress test duration must be greater than 0 minutes");
        
        // Memory limit validation
        addRule("memoryLimit",
            [](const TestConfig::VisualizationTestConfig& config) { 
                return config.stress.peakMemoryLimitMB > 0; 
            },
            "Memory limit must be greater than 0 MB");
    }

    void addVisualizationRules() {
        // Dimensions validation
        addRule("dimensions",
            [](const TestConfig::VisualizationTestConfig& config) { 
                return config.visualization.width > 0 && 
                       config.visualization.height > 0; 
            },
            "Visualization dimensions must be greater than 0");
        
        // Theme validation
        addRule("theme",
            [](const TestConfig::VisualizationTestConfig& config) { 
                return !config.visualization.theme.empty(); 
            },
            "Theme must be specified");

        // Required elements validation
        addRule("requiredElements",
            [](const TestConfig::VisualizationTestConfig& config) { 
                return !config.validation.requiredElements.empty(); 
            },
            "Required elements list cannot be empty");
    }

    static bool isValidPath(const std::string& path) {
        try {
            return std::filesystem::path(path).has_filename();
        } catch (...) {
            return false;
        }
    }
};

} // namespace test
} // namespace meshtastic

// Helper macros for configuration validation
#define VALIDATE_CONFIG(config) \
    meshtastic::test::ConfigValidator::instance().validate(config)

#define PRINT_VALIDATION_REPORT(result) \
    printf("%s", meshtastic::test::ConfigValidator::instance().generateReport(result).c_str())