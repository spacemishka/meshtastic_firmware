#include "memory_visualizer.h"
#include "memory_visualizer_components.h"
#include "memory_visualizer_interactive.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <future>
#include <vector>
#include <fstream>

/**
 * Stress testing for memory visualization system
 */
class VisualizationStressTest {
public:
    struct StressConfig {
        size_t threadCount = 4;
        size_t iterationCount = 1000;
        size_t dataSetSize = 10000;
        size_t peakMemoryLimit = 1024 * 1024 * 1024;  // 1GB
        std::chrono::seconds duration{60};  // 1 minute
        bool checkMemoryLeaks = true;
        bool validateOutput = true;
    };

    struct StressResults {
        size_t successfulIterations = 0;
        size_t failedIterations = 0;
        size_t peakMemoryUsage = 0;
        std::chrono::milliseconds averageResponseTime{0};
        std::vector<std::string> errors;
        bool completed = false;
    };

    static StressResults runStressTest(const StressConfig& config) {
        StressResults results;
        std::atomic<bool> shouldStop{false};
        std::atomic<size_t> currentMemoryUsage{0};
        std::vector<std::thread> threads;
        std::vector<std::future<void>> futures;
        
        // Start memory monitor thread
        auto memoryMonitor = std::async(std::launch::async, [&]() {
            monitorMemoryUsage(currentMemoryUsage, config.peakMemoryLimit, shouldStop);
        });

        // Start worker threads
        for (size_t i = 0; i < config.threadCount; i++) {
            futures.push_back(std::async(std::launch::async, [&, i]() {
                workerThread(i, config, results, shouldStop, currentMemoryUsage);
            }));
        }

        // Wait for test duration
        std::this_thread::sleep_for(config.duration);
        shouldStop = true;

        // Wait for all threads to complete
        for (auto& future : futures) {
            future.wait();
        }

        // Check for memory leaks if enabled
        if (config.checkMemoryLeaks) {
            checkForMemoryLeaks(results);
        }

        results.completed = true;
        return results;
    }

private:
    static void workerThread(size_t threadId,
                           const StressConfig& config,
                           StressResults& results,
                           std::atomic<bool>& shouldStop,
                           std::atomic<size_t>& currentMemoryUsage) {
        std::mt19937 rng(threadId);
        std::uniform_int_distribution<size_t> sizeDist(64, 16384);
        
        MemoryVisualizer::VisualizationConfig visConfig;
        visConfig.width = 800;
        visConfig.height = 600;
        
        for (size_t i = 0; i < config.iterationCount && !shouldStop; i++) {
            try {
                auto start = std::chrono::steady_clock::now();
                
                // Generate random test data
                generateTestData(config.dataSetSize / config.threadCount, sizeDist(rng));
                
                // Perform visualization
                std::stringstream output;
                MemoryVisualizer::instance().generateVisualization(output, visConfig);
                
                auto end = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                
                // Update metrics
                {
                    std::lock_guard<std::mutex> lock(results.mutex);
                    results.successfulIterations++;
                    results.averageResponseTime = std::chrono::milliseconds(
                        (results.averageResponseTime.count() * (results.successfulIterations - 1) +
                         duration.count()) / results.successfulIterations
                    );
                }
                
                // Validate output if enabled
                if (config.validateOutput) {
                    validateVisualization(output.str(), results);
                }
                
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(results.mutex);
                results.failedIterations++;
                results.errors.push_back(
                    "Thread " + std::to_string(threadId) + ": " + e.what()
                );
            }
        }
    }

    static void monitorMemoryUsage(std::atomic<size_t>& currentMemoryUsage,
                                 size_t peakMemoryLimit,
                                 std::atomic<bool>& shouldStop) {
        while (!shouldStop) {
            size_t usage = getCurrentMemoryUsage();
            currentMemoryUsage = usage;
            
            if (usage > peakMemoryLimit) {
                shouldStop = true;
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    static void generateTestData(size_t count, size_t maxSize) {
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<size_t> sizeDist(64, maxSize);
        std::uniform_int_distribution<int> lineDist(1, 1000);
        
        auto& analyzer = AllocationPatternAnalyzer::instance();
        
        for (size_t i = 0; i < count; i++) {
            void* ptr = malloc(sizeDist(rng));
            analyzer.recordAllocation(ptr, sizeDist(rng), "stress_test.cpp", lineDist(rng));
            
            if (i % 3 == 0) {
                analyzer.recordDeallocation(ptr);
                free(ptr);
            }
        }
    }

    static void validateVisualization(const std::string& svg, StressResults& results) {
        try {
            // Check SVG structure
            if (svg.find("<?xml") == std::string::npos ||
                svg.find("<svg") == std::string::npos ||
                svg.find("</svg>") == std::string::npos) {
                throw std::runtime_error("Invalid SVG structure");
            }

            // Check for required elements
            std::vector<std::string> requiredElements = {
                "<g", "<path", "<rect", "<text"
            };
            
            for (const auto& element : requiredElements) {
                if (svg.find(element) == std::string::npos) {
                    throw std::runtime_error("Missing required element: " + element);
                }
            }

            // Check for script inclusion if interactive
            if (svg.find("script") == std::string::npos) {
                throw std::runtime_error("Missing interactive features");
            }

        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(results.mutex);
            results.errors.push_back("Validation error: " + std::string(e.what()));
            results.failedIterations++;
        }
    }

    static void checkForMemoryLeaks(StressResults& results) {
        auto& analyzer = AllocationPatternAnalyzer::instance();
        auto patterns = analyzer.analyzePatterns();
        
        for (const auto& pattern : patterns) {
            if (pattern.type == AllocationPatternAnalyzer::PatternInfo::Type::LEAK_LIKELY) {
                std::lock_guard<std::mutex> lock(results.mutex);
                results.errors.push_back(
                    "Potential memory leak detected: " + pattern.description
                );
            }
        }
    }

    static size_t getCurrentMemoryUsage() {
        #ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX pmc;
        GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
        return pmc.WorkingSetSize;
        #else
        std::ifstream statm("/proc/self/statm");
        size_t size, resident, share, text, lib, data, dt;
        statm >> size >> resident >> share >> text >> lib >> data >> dt;
        return resident * sysconf(_SC_PAGE_SIZE);
        #endif
    }
};

int main() {
    VisualizationStressTest::StressConfig config;
    config.threadCount = std::thread::hardware_concurrency();
    config.iterationCount = 1000;
    config.dataSetSize = 10000;
    config.duration = std::chrono::minutes(5);
    
    std::cout << "Starting stress test with " << config.threadCount << " threads...\n";
    
    auto results = VisualizationStressTest::runStressTest(config);
    
    std::cout << "\nStress Test Results:\n"
              << "==================\n"
              << "Successful iterations: " << results.successfulIterations << "\n"
              << "Failed iterations: " << results.failedIterations << "\n"
              << "Average response time: " << results.averageResponseTime.count() << "ms\n"
              << "Peak memory usage: " << (results.peakMemoryUsage / 1024 / 1024) << "MB\n\n";
    
    if (!results.errors.empty()) {
        std::cout << "Errors encountered:\n";
        for (const auto& error : results.errors) {
            std::cout << "- " << error << "\n";
        }
    }
    
    return results.failedIterations > 0 ? 1 : 0;
}