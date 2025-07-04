#pragma once

#include "memory_visualizer.h"

class MemoryVisualizerComponents {
public:
    static void generateLegend(std::ofstream& file, const MemoryVisualizer::VisualizationConfig& config) {
        const int legendY = config.height - 30;
        
        file << "<g transform=\"translate(" << config.margin << "," 
             << legendY << ")\" class=\"legend\">\n";
        
        // Memory usage legend
        generateLegendItem(file, 0, "#2196F3", "Memory Usage");
        generateLegendItem(file, 100, "#4CAF50", "Allocated Blocks");
        generateLegendItem(file, 200, "#FF5722", "Free Blocks");
        generateLegendItem(file, 300, "#FFC107", "Fragmented Areas");
        
        file << "</g>\n";
    }

    static void generatePieChart(std::ofstream& file,
                               const std::vector<AllocationPatternAnalyzer::PatternInfo>& patterns,
                               int height) {
        const int radius = height / 2 - 20;
        const int centerX = radius + 20;
        const int centerY = radius + 20;
        
        float total = 0;
        for (const auto& pattern : patterns) {
            total += pattern.confidence;
        }

        float startAngle = 0;
        int index = 0;
        
        for (const auto& pattern : patterns) {
            float angle = (pattern.confidence / total) * 2 * M_PI;
            
            // Calculate arc path
            float x1 = centerX + radius * cos(startAngle);
            float y1 = centerY + radius * sin(startAngle);
            float x2 = centerX + radius * cos(startAngle + angle);
            float y2 = centerY + radius * sin(startAngle + angle);
            
            // Generate SVG path
            file << "<path d=\"M " << centerX << "," << centerY 
                 << " L " << x1 << "," << y1 
                 << " A " << radius << "," << radius << " 0 "
                 << (angle > M_PI ? "1" : "0") << ",1 "
                 << x2 << "," << y2 
                 << " Z\" fill=\"" << getPatternColor(index) << "\""
                 << " class=\"pattern-slice\">\n";
            
            // Add tooltip
            if (pattern.description.length() > 0) {
                file << "  <title>" << pattern.description << "</title>\n";
            }
            
            file << "</path>\n";
            
            startAngle += angle;
            index++;
        }
    }

    static void generateFragmentationMetrics(std::ofstream& file,
                                          const HeapFragmentationAnalyzer::FragmentationInfo& info) {
        const int metricsY = 20;
        
        file << "<g transform=\"translate(0," << metricsY << ")\">\n";
        
        // Memory usage bar
        generateProgressBar(file, 0, 0, 200, 20,
                          static_cast<float>(info.usedMemory) / info.totalHeapSize,
                          "#2196F3", "Memory Usage");
                          
        // Fragmentation index bar
        generateProgressBar(file, 0, 30, 200, 20,
                          info.fragmentationIndex,
                          "#FF5722", "Fragmentation");
                          
        // Block count text
        file << "<text x=\"0\" y=\"70\" class=\"metric-text\">"
             << "Fragments: " << info.totalFragments
             << "</text>\n";
             
        // Largest block text
        file << "<text x=\"0\" y=\"90\" class=\"metric-text\">"
             << "Largest Free: " << formatSize(info.largestFreeBlock)
             << "</text>\n";
             
        file << "</g>\n";
    }

    static void generateBlockDistribution(std::ofstream& file,
                                       const HeapFragmentationAnalyzer::FragmentationInfo& info) {
        const int histogramY = 120;
        const int barWidth = 15;
        const int maxHeight = 100;
        
        file << "<g transform=\"translate(0," << histogramY << ")\">\n";
        
        // Generate histogram of block sizes
        size_t maxCount = 0;
        std::map<size_t, size_t> sizeBuckets;
        
        // Group blocks into size buckets
        for (size_t size : info.freeBlockSizes) {
            size_t bucket = log2(size);
            sizeBuckets[bucket]++;
            maxCount = std::max(maxCount, sizeBuckets[bucket]);
        }
        
        // Draw bars
        int x = 0;
        for (const auto& [bucket, count] : sizeBuckets) {
            float height = (static_cast<float>(count) / maxCount) * maxHeight;
            
            file << "<rect x=\"" << x << "\" y=\"" 
                 << (maxHeight - height) << "\" width=\"" 
                 << barWidth << "\" height=\"" << height 
                 << "\" fill=\"#2196F3\">\n";
                 
            file << "  <title>" << formatSize(1ULL << bucket) 
                 << ": " << count << " blocks</title>\n";
                 
            file << "</rect>\n";
            
            x += barWidth + 2;
        }
        
        file << "</g>\n";
    }

    static void generateAnimatedTimeline(std::ofstream& file,
                                      const MemoryVisualizer::VisualizationConfig& config) {
        file << "<g class=\"timeline\">\n";
        
        // Get allocation events
        const auto& events = AllocationPatternAnalyzer::instance().getAllocationEvents();
        
        // Generate timeline path with animation
        std::stringstream path;
        path << "<path d=\"";
        
        size_t currentMemory = 0;
        for (const auto& event : events) {
            if (event.isAllocation) {
                currentMemory += event.size;
            } else {
                currentMemory -= event.size;
            }
            
            path << "L " << scaleTimeToX(event.timestamp, config) 
                 << " " << scaleMemoryToY(currentMemory, config) << " ";
        }
        
        path << "\" stroke=\"#2196F3\" fill=\"none\""
             << " stroke-dasharray=\"1000\""
             << " stroke-dashoffset=\"1000\">\n"
             << "  <animate attributeName=\"stroke-dashoffset\""
             << " from=\"1000\" to=\"0\""
             << " dur=\"2s\" fill=\"freeze\"/>\n"
             << "</path>\n";
             
        file << path.str();
        file << "</g>\n";
    }

private:
    static void generateLegendItem(std::ofstream& file, int x, 
                                 const std::string& color, 
                                 const std::string& label) {
        file << "<rect x=\"" << x << "\" y=\"0\" width=\"15\" height=\"15\""
             << " fill=\"" << color << "\"/>\n";
        file << "<text x=\"" << (x + 20) << "\" y=\"12\">" 
             << label << "</text>\n";
    }

    static void generateProgressBar(std::ofstream& file,
                                  int x, int y, int width, int height,
                                  float value, const std::string& color,
                                  const std::string& label) {
        file << "<g transform=\"translate(" << x << "," << y << ")\">\n";
        
        // Background
        file << "<rect width=\"" << width << "\" height=\"" << height 
             << "\" fill=\"#eee\"/>\n";
             
        // Progress
        file << "<rect width=\"" << (width * value) 
             << "\" height=\"" << height 
             << "\" fill=\"" << color << "\">\n";
             
        // Animate on load
        file << "  <animate attributeName=\"width\" from=\"0\" to=\""
             << (width * value) << "\" dur=\"1s\" fill=\"freeze\"/>\n";
             
        file << "</rect>\n";
        
        // Label
        file << "<text x=\"5\" y=\"" << (height - 5) 
             << "\" fill=\"white\">" << label << ": "
             << static_cast<int>(value * 100) << "%</text>\n";
             
        file << "</g>\n";
    }

    static std::string getPatternColor(int index) {
        static const std::vector<std::string> colors = {
            "#2196F3", "#4CAF50", "#FF5722", "#FFC107",
            "#9C27B0", "#00BCD4", "#FF9800", "#607D8B"
        };
        return colors[index % colors.size()];
    }

    static std::string formatSize(size_t bytes) {
        static const char* units[] = {"B", "KB", "MB", "GB"};
        int unit = 0;
        double size = bytes;
        
        while (size >= 1024 && unit < 3) {
            size /= 1024;
            unit++;
        }
        
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << size << " " << units[unit];
        return ss.str();
    }

    static double scaleTimeToX(uint64_t timestamp,
                             const MemoryVisualizer::VisualizationConfig& config) {
        // Implementation depends on time range calculations
        return 0.0; // Placeholder
    }

    static double scaleMemoryToY(size_t bytes,
                               const MemoryVisualizer::VisualizationConfig& config) {
        // Implementation depends on memory range calculations
        return 0.0; // Placeholder
    }
};