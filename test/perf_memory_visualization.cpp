#include "memory_visualizer.h"
#include "memory_visualizer_components.h"
#include "memory_visualizer_interactive.h"
#include <benchmark/benchmark.h>
#include <sstream>
#include <random>

/**
 * Performance tests for memory visualization system
 */
class VisualizationPerformanceTest : public benchmark::Fixture {
protected:
    MemoryVisualizer::VisualizationConfig config;
    std::stringstream output;
    std::mt19937 rng;

    void SetUp(const benchmark::State& state) {
        config.width = 1200;
        config.height = 800;
        config.margin = 50;
        config.showGrid = true;
        config.showTooltips = true;

        // Generate test data based on state
        generateTestData(state.range(0));
    }

    void generateTestData(size_t count) {
        std::uniform_int_distribution<size_t> sizeDist(64, 16384);
        std::uniform_int_distribution<int> lineDist(1, 1000);
        
        auto& analyzer = AllocationPatternAnalyzer::instance();
        for (size_t i = 0; i < count; i++) {
            void* ptr = malloc(sizeDist(rng));
            analyzer.recordAllocation(ptr, sizeDist(rng), "test.cpp", lineDist(rng));
            
            // Free some allocations to create fragmentation
            if (i % 3 == 0) {
                analyzer.recordDeallocation(ptr);
                free(ptr);
            }
        }
    }

    void TearDown(const benchmark::State& state) {
        // Clear test data
        AllocationPatternAnalyzer::instance().reset();
        HeapFragmentationAnalyzer::instance().reset();
    }
};

// Benchmark basic visualization generation
BENCHMARK_DEFINE_F(VisualizationPerformanceTest, BasicVisualization)
(benchmark::State& state) {
    for (auto _ : state) {
        output.str("");
        output.clear();
        MemoryVisualizer::instance().generateVisualization(output, config);
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * output.str().size());
}

// Benchmark timeline generation
BENCHMARK_DEFINE_F(VisualizationPerformanceTest, TimelineGeneration)
(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        output.str("");
        output.clear();
        state.ResumeTiming();
        
        MemoryVisualizerComponents::generateAnimatedTimeline(output, config);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Benchmark fragmentation visualization
BENCHMARK_DEFINE_F(VisualizationPerformanceTest, FragmentationVisualization)
(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        output.str("");
        output.clear();
        auto info = HeapFragmentationAnalyzer::instance().analyze();
        state.ResumeTiming();
        
        MemoryVisualizerComponents::generateFragmentationMetrics(output, info);
        MemoryVisualizerComponents::generateBlockDistribution(output, info);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Benchmark pattern detection and visualization
BENCHMARK_DEFINE_F(VisualizationPerformanceTest, PatternVisualization)
(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        output.str("");
        output.clear();
        auto patterns = AllocationPatternAnalyzer::instance().analyzePatterns();
        state.ResumeTiming();
        
        MemoryVisualizerComponents::generatePieChart(output, patterns, config.height / 3);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Benchmark interactive feature generation
BENCHMARK_DEFINE_F(VisualizationPerformanceTest, InteractiveFeatures)
(benchmark::State& state) {
    MemoryVisualizerInteractive::InteractionConfig interactiveConfig;
    
    for (auto _ : state) {
        output.str("");
        output.clear();
        MemoryVisualizerInteractive::generateInteractiveElements(output, config, interactiveConfig);
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * output.str().size());
}

// Benchmark memory-intensive visualization
BENCHMARK_DEFINE_F(VisualizationPerformanceTest, MemoryIntensive)
(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        std::vector<std::string> fragments;
        fragments.reserve(state.range(0));
        state.ResumeTiming();
        
        output.str("");
        output.clear();
        MemoryVisualizer::instance().generateVisualization(output, config);
        
        // Simulate memory pressure by storing fragments
        std::string svg = output.str();
        size_t pos = 0;
        while ((pos = svg.find(">", pos)) != std::string::npos) {
            fragments.push_back(svg.substr(0, pos + 1));
            pos++;
        }
        
        benchmark::DoNotOptimize(fragments);
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * output.str().size());
}

// Register benchmarks with different data sizes
BENCHMARK_REGISTER_F(VisualizationPerformanceTest, BasicVisualization)
    ->RangeMultiplier(4)
    ->Range(64, 16384)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

BENCHMARK_REGISTER_F(VisualizationPerformanceTest, TimelineGeneration)
    ->RangeMultiplier(4)
    ->Range(64, 16384)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

BENCHMARK_REGISTER_F(VisualizationPerformanceTest, FragmentationVisualization)
    ->RangeMultiplier(4)
    ->Range(64, 16384)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

BENCHMARK_REGISTER_F(VisualizationPerformanceTest, PatternVisualization)
    ->RangeMultiplier(4)
    ->Range(64, 16384)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

BENCHMARK_REGISTER_F(VisualizationPerformanceTest, InteractiveFeatures)
    ->RangeMultiplier(4)
    ->Range(64, 16384)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

BENCHMARK_REGISTER_F(VisualizationPerformanceTest, MemoryIntensive)
    ->RangeMultiplier(4)
    ->Range(64, 16384)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime()
    ->MeasureProcessCPUTime()
    ->MeasurePeakMemoryUsage();

BENCHMARK_MAIN();