#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include "allocation_pattern.h"
#include "heap_fragmentation.h"

/**
 * Memory visualization tool for analyzing memory patterns
 */
class MemoryVisualizer {
public:
    // SVG generation settings
    struct VisualizationConfig {
        int width = 1200;           // SVG width
        int height = 800;           // SVG height
        int margin = 50;            // Chart margin
        bool showGrid = true;       // Show background grid
        bool showTooltips = true;   // Show interactive tooltips
        std::string colorScheme = "default";  // Color scheme name
    };

    static MemoryVisualizer& instance() {
        static MemoryVisualizer visualizer;
        return visualizer;
    }

    void generateVisualization(const char* filename, 
                             const VisualizationConfig& config = VisualizationConfig()) {
        std::ofstream file(filename);
        if (!file) return;

        // Get data from analyzers
        auto patterns = AllocationPatternAnalyzer::instance().analyzePatterns();
        auto fragInfo = HeapFragmentationAnalyzer::instance().analyze();

        // Generate SVG
        generateSVGHeader(file, config);
        generateTimelinePlot(file, config);
        generateAllocationMap(file, config);
        generatePatternChart(file, config, patterns);
        generateFragmentationView(file, config, fragInfo);
        generateLegend(file, config);
        generateSVGFooter(file);

        file.close();
    }

    void generateAnimatedView(const char* filename) {
        std::ofstream file(filename);
        if (!file) return;

        // Generate animated SVG showing memory changes over time
        file << "<!DOCTYPE html>\n<html><body>\n";
        file << "<style>\n";
        generateStyles(file);
        file << "</style>\n";

        VisualizationConfig config;
        generateSVGHeader(file, config);

        // Add animation definitions
        file << "<defs>\n";
        generateAnimations(file);
        file << "</defs>\n";

        // Generate animated components
        generateAnimatedTimeline(file, config);
        generateAnimatedAllocationMap(file, config);
        generateAnimatedMetrics(file, config);

        generateSVGFooter(file);
        file << "</body></html>\n";
        file.close();
    }

private:
    void generateSVGHeader(std::ofstream& file, const VisualizationConfig& config) {
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        file << "<svg width=\"" << config.width << "\" height=\"" << config.height 
             << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";
        
        if (config.showGrid) {
            generateGrid(file, config);
        }
    }

    void generateGrid(std::ofstream& file, const VisualizationConfig& config) {
        file << "<defs>\n";
        file << "  <pattern id=\"grid\" width=\"20\" height=\"20\" "
             << "patternUnits=\"userSpaceOnUse\">\n";
        file << "    <path d=\"M 20 0 L 0 0 0 20\" fill=\"none\" "
             << "stroke=\"#eee\" stroke-width=\"0.5\"/>\n";
        file << "  </pattern>\n";
        file << "</defs>\n";
        file << "<rect width=\"100%\" height=\"100%\" fill=\"url(#grid)\"/>\n";
    }

    void generateTimelinePlot(std::ofstream& file, const VisualizationConfig& config) {
        const int plotHeight = config.height / 3;
        const int plotWidth = config.width - 2 * config.margin;

        file << "<g transform=\"translate(" << config.margin << "," 
             << config.margin << ")\">\n";
        
        // Draw axes
        file << "  <line x1=\"0\" y1=\"" << plotHeight << "\" x2=\"" 
             << plotWidth << "\" y2=\"" << plotHeight 
             << "\" stroke=\"black\"/>\n";
        file << "  <line x1=\"0\" y1=\"0\" x2=\"0\" y2=\"" 
             << plotHeight << "\" stroke=\"black\"/>\n";

        // Plot memory usage over time
        generateMemoryUsagePath(file, plotWidth, plotHeight);

        file << "</g>\n";
    }

    void generateMemoryUsagePath(std::ofstream& file, int width, int height) {
        auto& analyzer = AllocationPatternAnalyzer::instance();
        const auto& allocations = analyzer.getAllocations();

        if (allocations.empty()) return;

        // Find time range
        auto timeRange = getTimeRange(allocations);
        auto memRange = getMemoryRange(allocations);

        std::stringstream path;
        path << "  <path d=\"M";
        bool first = true;

        size_t currentMemory = 0;
        for (const auto& [time, record] : allocations) {
            if (record.isAllocation) {
                currentMemory += record.size;
            } else {
                currentMemory -= record.size;
            }

            if (first) {
                path << scaleX(time, timeRange, width) << " "
                     << scaleY(currentMemory, memRange, height);
                first = false;
            } else {
                path << " L " << scaleX(time, timeRange, width) << " "
                     << scaleY(currentMemory, memRange, height);
            }
        }

        path << "\" fill=\"none\" stroke=\"#2196F3\" stroke-width=\"2\"/>\n";
        file << path.str();
    }

    void generateAllocationMap(std::ofstream& file, const VisualizationConfig& config) {
        const int mapTop = config.height / 3 + 2 * config.margin;
        const int mapHeight = config.height / 3;
        const int mapWidth = config.width - 2 * config.margin;

        file << "<g transform=\"translate(" << config.margin << "," 
             << mapTop << ")\">\n";

        // Generate memory map rectangles
        auto& fragAnalyzer = HeapFragmentationAnalyzer::instance();
        const auto& blocks = fragAnalyzer.getMemoryBlocks();

        if (!blocks.empty()) {
            const auto totalSize = fragAnalyzer.getTotalHeapSize();
            for (const auto& block : blocks) {
                int x = (block.address * mapWidth) / totalSize;
                int width = (block.size * mapWidth) / totalSize;
                
                std::string color = block.isUsed ? "#4CAF50" : "#FF5722";
                file << "  <rect x=\"" << x << "\" y=\"0\" width=\"" 
                     << width << "\" height=\"" << mapHeight 
                     << "\" fill=\"" << color << "\"";
                
                if (config.showTooltips) {
                    file << " onmouseover=\"showTooltip(evt, '" 
                         << (block.isUsed ? "Used: " : "Free: ")
                         << block.size << " bytes')\"";
                }
                
                file << "/>\n";
            }
        }

        file << "</g>\n";
    }

    void generatePatternChart(std::ofstream& file, 
                            const VisualizationConfig& config,
                            const std::vector<AllocationPatternAnalyzer::PatternInfo>& patterns) {
        const int chartTop = 2 * config.height / 3 + 2 * config.margin;
        const int chartHeight = config.height / 3 - config.margin;

        file << "<g transform=\"translate(" << config.margin << "," 
             << chartTop << ")\">\n";

        // Generate pattern distribution pie chart
        if (!patterns.empty()) {
            generatePieChart(file, patterns, chartHeight);
        }

        file << "</g>\n";
    }

    void generateFragmentationView(std::ofstream& file,
                                 const VisualizationConfig& config,
                                 const HeapFragmentationAnalyzer::FragmentationInfo& fragInfo) {
        const int viewTop = 2 * config.height / 3 + 2 * config.margin;
        const int viewLeft = config.width / 2 + config.margin;

        file << "<g transform=\"translate(" << viewLeft << "," 
             << viewTop << ")\">\n";

        // Generate fragmentation visualization
        generateFragmentationMetrics(file, fragInfo);
        generateBlockDistribution(file, fragInfo);

        file << "</g>\n";
    }

    void generateAnimations(std::ofstream& file) {
        file << "<style type=\"text/css\">\n";
        file << "@keyframes memoryGrow {\n";
        file << "  from { transform: scaleY(0); }\n";
        file << "  to { transform: scaleY(1); }\n";
        file << "}\n";
        file << ".animated-block {\n";
        file << "  animation: memoryGrow 0.5s ease-out;\n";
        file << "}\n";
        file << "</style>\n";
    }

    void generateStyles(std::ofstream& file) {
        file << "svg { font-family: sans-serif; }\n";
        file << ".axis { stroke: #333; stroke-width: 1; }\n";
        file << ".label { font-size: 12px; fill: #666; }\n";
        file << ".tooltip { position: absolute; padding: 8px; "
             << "background: rgba(0,0,0,0.8); color: white; "
             << "border-radius: 4px; font-size: 12px; }\n";
    }

    void generateSVGFooter(std::ofstream& file) {
        if (file.is_open()) {
            file << "</svg>\n";
        }
    }

    // Utility functions
    double scaleX(uint64_t time, const std::pair<uint64_t, uint64_t>& range, int width) {
        return (static_cast<double>(time - range.first) / 
                (range.second - range.first)) * width;
    }

    double scaleY(size_t value, const std::pair<size_t, size_t>& range, int height) {
        return height - (static_cast<double>(value - range.first) / 
                        (range.second - range.first)) * height;
    }

    std::pair<uint64_t, uint64_t> getTimeRange(
        const std::map<uint64_t, AllocationPatternAnalyzer::AllocationRecord>& allocations) {
        if (allocations.empty()) return {0, 0};
        return {allocations.begin()->first, allocations.rbegin()->first};
    }

    std::pair<size_t, size_t> getMemoryRange(
        const std::map<uint64_t, AllocationPatternAnalyzer::AllocationRecord>& allocations) {
        size_t maxMem = 0;
        size_t currentMem = 0;
        
        for (const auto& [_, record] : allocations) {
            if (record.isAllocation) {
                currentMem += record.size;
            } else {
                currentMem -= record.size;
            }
            maxMem = std::max(maxMem, currentMem);
        }
        
        return {0, maxMem};
    }
};

// Macros for visualization
#define VISUALIZE_MEMORY(filename) \
    MemoryVisualizer::instance().generateVisualization(filename)

#define VISUALIZE_MEMORY_ANIMATED(filename) \
    MemoryVisualizer::instance().generateAnimatedView(filename)