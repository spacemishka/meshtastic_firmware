#pragma once

#include <string>
#include <map>
#include <filesystem>
#include <fstream>
#include "json_config.h"

/**
 * Test configuration for memory visualization system
 */
class TestConfig {
public:
    struct VisualizationTestConfig {
        // Basic settings
        size_t testDataSize = 1000;
        size_t iterations = 100;
        bool enableLogging = true;
        std::string outputDir = "test_output";

        // Performance test settings
        struct {
            size_t threadCount = 4;
            size_t minDataSize = 64;
            size_t maxDataSize = 16384;
            int timeoutSeconds = 300;
            bool measureMemory = true;
        } performance;

        // Stress test settings
        struct {
            size_t concurrentThreads = 8;
            size_t durationMinutes = 5;
            size_t peakMemoryLimitMB = 1024;
            bool abortOnError = true;
        } stress;

        // Visualization settings
        struct {
            int width = 1200;
            int height = 800;
            std::string theme = "default";
            bool enableAnimations = true;
            bool enableInteractive = true;
        } visualization;

        // Validation settings
        struct {
            bool validateSVG = true;
            bool checkMemoryLeaks = true;
            bool verifyOutput = true;
            std::vector<std::string> requiredElements = {
                "svg", "g", "path", "rect", "text"
            };
        } validation;
    };

    static TestConfig& instance() {
        static TestConfig config;
        return config;
    }

    bool load(const std::string& filename) {
        try {
            std::ifstream file(filename);
            if (!file.is_open()) {
                createDefaultConfig(filename);
                return false;
            }

            std::string jsonStr((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
            JsonValue root = JsonValue::parse(jsonStr);

            // Load basic settings
            config.testDataSize = CONFIG_VALUE(root, "testDataSize", Number, config.testDataSize).asInt64();
            config.iterations = CONFIG_VALUE(root, "iterations", Number, config.iterations).asInt64();
            config.enableLogging = CONFIG_VALUE(root, "enableLogging", Boolean, config.enableLogging).asBool();
            config.outputDir = CONFIG_VALUE(root, "outputDir", String, config.outputDir).asString();

            // Load performance settings
            const auto& perf = root["performance"];
            config.performance.threadCount = CONFIG_VALUE(perf, "threadCount", Number, 
                config.performance.threadCount).asInt64();
            config.performance.minDataSize = CONFIG_VALUE(perf, "minDataSize", Number, 
                config.performance.minDataSize).asInt64();
            config.performance.maxDataSize = CONFIG_VALUE(perf, "maxDataSize", Number, 
                config.performance.maxDataSize).asInt64();
            config.performance.timeoutSeconds = CONFIG_VALUE(perf, "timeoutSeconds", Number, 
                config.performance.timeoutSeconds).asInt64();
            config.performance.measureMemory = CONFIG_VALUE(perf, "measureMemory", Boolean, 
                config.performance.measureMemory).asBool();

            // Load stress settings
            const auto& stress = root["stress"];
            config.stress.concurrentThreads = CONFIG_VALUE(stress, "concurrentThreads", Number, 
                config.stress.concurrentThreads).asInt64();
            config.stress.durationMinutes = CONFIG_VALUE(stress, "durationMinutes", Number, 
                config.stress.durationMinutes).asInt64();
            config.stress.peakMemoryLimitMB = CONFIG_VALUE(stress, "peakMemoryLimitMB", Number, 
                config.stress.peakMemoryLimitMB).asInt64();
            config.stress.abortOnError = CONFIG_VALUE(stress, "abortOnError", Boolean, 
                config.stress.abortOnError).asBool();

            // Load visualization settings
            const auto& vis = root["visualization"];
            config.visualization.width = CONFIG_VALUE(vis, "width", Number, 
                config.visualization.width).asInt64();
            config.visualization.height = CONFIG_VALUE(vis, "height", Number, 
                config.visualization.height).asInt64();
            config.visualization.theme = CONFIG_VALUE(vis, "theme", String, 
                config.visualization.theme).asString();
            config.visualization.enableAnimations = CONFIG_VALUE(vis, "enableAnimations", Boolean, 
                config.visualization.enableAnimations).asBool();
            config.visualization.enableInteractive = CONFIG_VALUE(vis, "enableInteractive", Boolean, 
                config.visualization.enableInteractive).asBool();

            // Load validation settings
            const auto& val = root["validation"];
            config.validation.validateSVG = CONFIG_VALUE(val, "validateSVG", Boolean, 
                config.validation.validateSVG).asBool();
            config.validation.checkMemoryLeaks = CONFIG_VALUE(val, "checkMemoryLeaks", Boolean, 
                config.validation.checkMemoryLeaks).asBool();
            config.validation.verifyOutput = CONFIG_VALUE(val, "verifyOutput", Boolean, 
                config.validation.verifyOutput).asBool();

            setupOutputDirectory();
            return true;

        } catch (const std::exception& e) {
            std::cerr << "Error loading test config: " << e.what() << std::endl;
            return false;
        }
    }

    const VisualizationTestConfig& getConfig() const {
        return config;
    }

    std::string getOutputPath(const std::string& filename) const {
        return (std::filesystem::path(config.outputDir) / filename).string();
    }

private:
    TestConfig() = default;

    void createDefaultConfig(const std::string& filename) {
        JsonValue root;

        // Basic settings
        root["testDataSize"] = JsonValue(static_cast<int64_t>(config.testDataSize));
        root["iterations"] = JsonValue(static_cast<int64_t>(config.iterations));
        root["enableLogging"] = JsonValue(config.enableLogging);
        root["outputDir"] = JsonValue(config.outputDir);

        // Performance settings
        JsonValue& perf = root["performance"];
        perf["threadCount"] = JsonValue(static_cast<int64_t>(config.performance.threadCount));
        perf["minDataSize"] = JsonValue(static_cast<int64_t>(config.performance.minDataSize));
        perf["maxDataSize"] = JsonValue(static_cast<int64_t>(config.performance.maxDataSize));
        perf["timeoutSeconds"] = JsonValue(static_cast<int64_t>(config.performance.timeoutSeconds));
        perf["measureMemory"] = JsonValue(config.performance.measureMemory);

        // Stress settings
        JsonValue& stress = root["stress"];
        stress["concurrentThreads"] = JsonValue(static_cast<int64_t>(config.stress.concurrentThreads));
        stress["durationMinutes"] = JsonValue(static_cast<int64_t>(config.stress.durationMinutes));
        stress["peakMemoryLimitMB"] = JsonValue(static_cast<int64_t>(config.stress.peakMemoryLimitMB));
        stress["abortOnError"] = JsonValue(config.stress.abortOnError);

        // Visualization settings
        JsonValue& vis = root["visualization"];
        vis["width"] = JsonValue(static_cast<int64_t>(config.visualization.width));
        vis["height"] = JsonValue(static_cast<int64_t>(config.visualization.height));
        vis["theme"] = JsonValue(config.visualization.theme);
        vis["enableAnimations"] = JsonValue(config.visualization.enableAnimations);
        vis["enableInteractive"] = JsonValue(config.visualization.enableInteractive);

        // Validation settings
        JsonValue& val = root["validation"];
        val["validateSVG"] = JsonValue(config.validation.validateSVG);
        val["checkMemoryLeaks"] = JsonValue(config.validation.checkMemoryLeaks);
        val["verifyOutput"] = JsonValue(config.validation.verifyOutput);

        // Write default config
        std::ofstream file(filename);
        file << root.toString();
    }

    void setupOutputDirectory() {
        std::filesystem::path dir(config.outputDir);
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }
    }

    VisualizationTestConfig config;
};

// Helper macros for test configuration
#define TEST_CONFIG TestConfig::instance().getConfig()
#define TEST_OUTPUT(filename) TestConfig::instance().getOutputPath(filename)