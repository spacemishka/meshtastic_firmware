#pragma once

#include <map>
#include <string>
#include <thread>
#include <functional>
#include "test_config.h"

namespace meshtastic {
namespace test {

/**
 * Predefined configuration presets with inheritance support
 */
class ConfigPresets {
public:
    // Preset types
    enum class PresetType {
        BASE,               // Base configuration
        QUICK_TEST,        // Fast tests with minimal data
        STANDARD_TEST,     // Normal test configuration
        FULL_TEST,        // Comprehensive testing
        PERFORMANCE_TEST, // Performance-focused testing
        STRESS_TEST,      // Stress testing configuration
        MEMORY_TEST,      // Memory-focused testing
        DEBUG_TEST,       // Debug configuration
        CI_TEST           // Continuous Integration testing
    };

    struct PresetInfo {
        PresetType basePreset;
        std::function<void(TestConfig::VisualizationTestConfig&)> customizer;
    };

    static ConfigPresets& instance() {
        static ConfigPresets presets;
        return presets;
    }

    TestConfig::VisualizationTestConfig getPreset(PresetType type) {
        // Start with base configuration
        auto config = createBasePreset();

        // Apply inherited configurations
        applyInheritance(config, type);

        return config;
    }

    TestConfig::VisualizationTestConfig getNamedPreset(const std::string& name) {
        auto it = customPresets.find(name);
        if (it != customPresets.end()) {
            auto config = getPreset(it->second.basePreset);
            it->second.customizer(config);
            return config;
        }
        return getPreset(PresetType::STANDARD_TEST);
    }

    void addCustomPreset(const std::string& name, 
                        PresetType basePreset,
                        std::function<void(TestConfig::VisualizationTestConfig&)> customizer) {
        customPresets[name] = {basePreset, customizer};
    }

    std::vector<std::string> listCustomPresets() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : customPresets) {
            names.push_back(name);
        }
        return names;
    }

private:
    std::map<std::string, PresetInfo> customPresets;
    const unsigned int hardwareConcurrency = std::thread::hardware_concurrency();

    void applyInheritance(TestConfig::VisualizationTestConfig& config, PresetType type) {
        switch (type) {
            case PresetType::QUICK_TEST:
                applyQuickTestSettings(config);
                break;
            case PresetType::STANDARD_TEST:
                applyStandardTestSettings(config);
                break;
            case PresetType::FULL_TEST:
                applyFullTestSettings(config);
                break;
            case PresetType::PERFORMANCE_TEST:
                applyPerformanceTestSettings(config);
                break;
            case PresetType::STRESS_TEST:
                applyStressTestSettings(config);
                break;
            case PresetType::MEMORY_TEST:
                applyMemoryTestSettings(config);
                break;
            case PresetType::DEBUG_TEST:
                applyDebugTestSettings(config);
                break;
            case PresetType::CI_TEST:
                applyCITestSettings(config);
                break;
            default:
                break;
        }
    }

    TestConfig::VisualizationTestConfig createBasePreset() {
        TestConfig::VisualizationTestConfig config;
        config.testDataSize = 100;
        config.iterations = 10;
        config.enableLogging = false;
        config.outputDir = "test_output";
        config.performance.threadCount = 1;
        config.performance.timeoutSeconds = 60;
        config.visualization.width = 800;
        config.visualization.height = 600;
        return config;
    }

    void applyQuickTestSettings(TestConfig::VisualizationTestConfig& config) {
        config.enableLogging = false;
        config.visualization.enableAnimations = false;
        config.validation.validateSVG = false;
    }

    void applyStandardTestSettings(TestConfig::VisualizationTestConfig& config) {
        config.testDataSize = 1000;
        config.iterations = 100;
        config.enableLogging = true;
        config.performance.threadCount = 2;
        config.performance.timeoutSeconds = 300;
        config.visualization.enableAnimations = true;
    }

    void applyFullTestSettings(TestConfig::VisualizationTestConfig& config) {
        config.testDataSize = 10000;
        config.iterations = 1000;
        config.enableLogging = true;
        config.performance.threadCount = hardwareConcurrency;
        config.performance.timeoutSeconds = 3600;
        config.visualization.width = 1920;
        config.visualization.height = 1080;
        config.validation.validateSVG = true;
        config.validation.checkMemoryLeaks = true;
    }

    void applyPerformanceTestSettings(TestConfig::VisualizationTestConfig& config) {
        config.testDataSize = 5000;
        config.iterations = 500;
        config.enableLogging = false;
        config.performance.threadCount = hardwareConcurrency;
        config.performance.minDataSize = 64;
        config.performance.maxDataSize = 16384;
        config.performance.measureMemory = true;
        config.visualization.enableAnimations = false;
    }

    void applyStressTestSettings(TestConfig::VisualizationTestConfig& config) {
        config.testDataSize = 50000;
        config.iterations = 5000;
        config.enableLogging = true;
        config.stress.concurrentThreads = hardwareConcurrency * 2;
        config.stress.durationMinutes = 60;
        config.stress.peakMemoryLimitMB = 2048;
        config.stress.abortOnError = true;
    }

    void applyMemoryTestSettings(TestConfig::VisualizationTestConfig& config) {
        config.testDataSize = 2000;
        config.iterations = 200;
        config.enableLogging = true;
        config.performance.measureMemory = true;
        config.validation.checkMemoryLeaks = true;
        config.stress.peakMemoryLimitMB = 1024;
    }

    void applyDebugTestSettings(TestConfig::VisualizationTestConfig& config) {
        config.testDataSize = 100;
        config.iterations = 10;
        config.enableLogging = true;
        config.performance.threadCount = 1;
        config.performance.timeoutSeconds = 3600;
        config.visualization.enableAnimations = false;
        config.validation.validateSVG = true;
        config.validation.checkMemoryLeaks = true;
        config.validation.verifyOutput = true;
    }

    void applyCITestSettings(TestConfig::VisualizationTestConfig& config) {
        config.testDataSize = 500;
        config.iterations = 50;
        config.enableLogging = true;
        config.performance.threadCount = 2;
        config.performance.timeoutSeconds = 600;
        config.visualization.enableAnimations = false;
        config.validation.validateSVG = true;
    }
};

} // namespace test
} // namespace meshtastic

// Helper macros for presets
#define GET_TEST_PRESET(type) \
    meshtastic::test::ConfigPresets::instance().getPreset(type)

#define GET_NAMED_PRESET(name) \
    meshtastic::test::ConfigPresets::instance().getNamedPreset(name)

#define ADD_CUSTOM_PRESET(name, base, customizer) \
    meshtastic::test::ConfigPresets::instance().addCustomPreset(name, base, customizer)