#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <algorithm>
#include <mutex>
#include "memory_validator.h"

/**
 * Allocation pattern analyzer for detecting memory usage patterns
 */
class AllocationPatternAnalyzer {
public:
    struct AllocationRecord {
        size_t size;
        const char* file;
        int line;
        std::chrono::steady_clock::time_point timestamp;
        void* address;
        bool isFreed;
        uint32_t stackHash;
    };

    struct PatternInfo {
        enum class Type {
            CYCLIC,          // Regular allocation/deallocation cycles
            GROWING,         // Steadily increasing memory usage
            SPIKES,         // Sudden spikes in allocation
            FRAGMENTED,      // Many small allocations
            LEAK_LIKELY,     // Possible memory leak pattern
            NORMAL          // No concerning pattern
        };

        Type type;
        double confidence;
        std::string description;
        std::vector<AllocationRecord> examples;
    };

    static AllocationPatternAnalyzer& instance() {
        static AllocationPatternAnalyzer analyzer;
        return analyzer;
    }

    void recordAllocation(void* ptr, size_t size, const char* file, int line) {
        std::lock_guard<std::mutex> lock(mutex);
        
        AllocationRecord record{
            size,
            file,
            line,
            std::chrono::steady_clock::now(),
            ptr,
            false,
            captureStackHash()
        };
        
        allocations[ptr] = record;
        updateMetrics(record);
    }

    void recordDeallocation(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex);
        
        auto it = allocations.find(ptr);
        if (it != allocations.end()) {
            it->second.isFreed = true;
            updateLifetimeStats(it->second);
        }
    }

    std::vector<PatternInfo> analyzePatterns() {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<PatternInfo> patterns;

        // Analyze allocation frequency patterns
        if (auto cyclicPattern = detectCyclicPattern()) {
            patterns.push_back(*cyclicPattern);
        }

        // Analyze growth patterns
        if (auto growthPattern = detectGrowthPattern()) {
            patterns.push_back(*growthPattern);
        }

        // Analyze fragmentation patterns
        if (auto fragPattern = detectFragmentationPattern()) {
            patterns.push_back(*fragPattern);
        }

        // Analyze potential leaks
        if (auto leakPattern = detectLeakPattern()) {
            patterns.push_back(*leakPattern);
        }

        return patterns;
    }

    void generateReport(const char* filename) {
        auto patterns = analyzePatterns();
        
        FILE* f = fopen(filename, "w");
        if (!f) return;

        fprintf(f, "=== Memory Allocation Pattern Analysis ===\n\n");

        // Print general statistics
        fprintf(f, "Total Allocations: %zu\n", metrics.totalAllocations);
        fprintf(f, "Average Allocation Size: %.2f bytes\n", metrics.averageSize);
        fprintf(f, "Peak Memory Usage: %zu bytes\n", metrics.peakUsage);
        fprintf(f, "Average Object Lifetime: %.2f ms\n\n", 
                metrics.averageLifetimeMs);

        // Print detected patterns
        fprintf(f, "Detected Patterns:\n");
        for (const auto& pattern : patterns) {
            fprintf(f, "\n%s (%.1f%% confidence)\n", 
                    patternTypeToString(pattern.type),
                    pattern.confidence * 100);
            fprintf(f, "%s\n", pattern.description.c_str());
            
            if (!pattern.examples.empty()) {
                fprintf(f, "\nExample allocations:\n");
                for (const auto& example : pattern.examples) {
                    fprintf(f, "  %zu bytes at %s:%d\n", 
                            example.size,
                            example.file,
                            example.line);
                }
            }
        }

        // Print hotspots
        fprintf(f, "\nAllocation Hotspots:\n");
        auto hotspots = findHotspots();
        for (const auto& hotspot : hotspots) {
            fprintf(f, "%s:%d - %zu allocations, %zu total bytes\n",
                    hotspot.file,
                    hotspot.line,
                    hotspot.count,
                    hotspot.totalSize);
        }

        fclose(f);
    }

private:
    struct Metrics {
        size_t totalAllocations = 0;
        size_t totalSize = 0;
        size_t peakUsage = 0;
        double averageSize = 0;
        double averageLifetimeMs = 0;
        std::vector<double> lifetimes;
        std::map<uint32_t, size_t> stackTraces;
    };

    struct Hotspot {
        const char* file;
        int line;
        size_t count;
        size_t totalSize;
    };

    std::optional<PatternInfo> detectCyclicPattern() {
        // Analyze allocation timestamps for regular patterns
        std::vector<double> intervals;
        auto it = allocations.begin();
        while (std::next(it) != allocations.end()) {
            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::next(it)->second.timestamp - it->second.timestamp).count();
            intervals.push_back(diff);
            ++it;
        }

        if (intervals.empty()) return std::nullopt;

        // Calculate standard deviation of intervals
        double mean = std::accumulate(intervals.begin(), intervals.end(), 0.0) / intervals.size();
        double variance = std::accumulate(intervals.begin(), intervals.end(), 0.0,
            [mean](double acc, double val) {
                double diff = val - mean;
                return acc + (diff * diff);
            }) / intervals.size();
        double stddev = std::sqrt(variance);

        // If standard deviation is low relative to mean, we have a cyclic pattern
        double coefficient = stddev / mean;
        if (coefficient < 0.3) {
            return PatternInfo{
                PatternInfo::Type::CYCLIC,
                1.0 - coefficient,
                "Regular allocation pattern detected with interval " + 
                std::to_string(static_cast<int>(mean)) + "ms",
                getExampleAllocations(3)
            };
        }

        return std::nullopt;
    }

    std::optional<PatternInfo> detectGrowthPattern() {
        // Track cumulative allocated memory over time
        std::vector<std::pair<std::chrono::steady_clock::time_point, size_t>> usage;
        size_t current = 0;
        
        for (const auto& [_, record] : allocations) {
            if (!record.isFreed) {
                current += record.size;
            }
            usage.emplace_back(record.timestamp, current);
        }

        if (usage.size() < 10) return std::nullopt;

        // Calculate growth rate
        double totalGrowth = static_cast<double>(usage.back().second - usage.front().second);
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            usage.back().first - usage.front().first).count();
        
        double growthRate = totalGrowth / duration;
        
        if (growthRate > 1024) { // More than 1KB/s growth
            return PatternInfo{
                PatternInfo::Type::GROWING,
                std::min(1.0, growthRate / 10240.0),
                "Memory usage growing at " + std::to_string(static_cast<int>(growthRate)) + " bytes/s",
                getExampleAllocations(3)
            };
        }

        return std::nullopt;
    }

    std::optional<PatternInfo> detectFragmentationPattern() {
        // Analyze size distribution of allocations
        std::map<size_t, size_t> sizeCounts;
        for (const auto& [_, record] : allocations) {
            sizeCounts[record.size]++;
        }

        // Calculate average allocation size
        double avgSize = metrics.averageSize;
        
        // Count small allocations
        size_t smallCount = 0;
        for (const auto& [size, count] : sizeCounts) {
            if (size < avgSize / 4) {
                smallCount += count;
            }
        }

        double smallRatio = static_cast<double>(smallCount) / metrics.totalAllocations;
        if (smallRatio > 0.5) {
            return PatternInfo{
                PatternInfo::Type::FRAGMENTED,
                smallRatio,
                std::to_string(static_cast<int>(smallRatio * 100)) + 
                "% of allocations are small (< " + 
                std::to_string(static_cast<int>(avgSize / 4)) + " bytes)",
                getExampleAllocations(3)
            };
        }

        return std::nullopt;
    }

    std::optional<PatternInfo> detectLeakPattern() {
        // Group allocations by stack trace
        std::map<uint32_t, std::vector<AllocationRecord>> traceGroups;
        
        for (const auto& [_, record] : allocations) {
            if (!record.isFreed) {
                traceGroups[record.stackHash].push_back(record);
            }
        }

        // Look for patterns indicating leaks
        for (const auto& [hash, records] : traceGroups) {
            if (records.size() > 10) {
                double avgLifetime = 0;
                for (const auto& record : records) {
                    if (record.isFreed) {
                        avgLifetime += std::chrono::duration_cast<std::chrono::seconds>(
                            record.timestamp - records[0].timestamp).count();
                    }
                }
                avgLifetime /= records.size();

                if (avgLifetime > 3600) { // Objects live longer than 1 hour
                    return PatternInfo{
                        PatternInfo::Type::LEAK_LIKELY,
                        std::min(1.0, records.size() / 100.0),
                        std::to_string(records.size()) + 
                        " objects allocated from same location, average lifetime " +
                        std::to_string(static_cast<int>(avgLifetime)) + "s",
                        records
                    };
                }
            }
        }

        return std::nullopt;
    }

    std::vector<Hotspot> findHotspots() {
        std::map<std::pair<const char*, int>, Hotspot> spots;
        
        for (const auto& [_, record] : allocations) {
            auto& spot = spots[{record.file, record.line}];
            spot.file = record.file;
            spot.line = record.line;
            spot.count++;
            spot.totalSize += record.size;
        }

        std::vector<Hotspot> result;
        for (const auto& [_, spot] : spots) {
            result.push_back(spot);
        }

        std::sort(result.begin(), result.end(),
            [](const Hotspot& a, const Hotspot& b) {
                return a.totalSize > b.totalSize;
            });

        return result;
    }

    void updateMetrics(const AllocationRecord& record) {
        metrics.totalAllocations++;
        metrics.totalSize += record.size;
        metrics.peakUsage = std::max(metrics.peakUsage, metrics.totalSize);
        metrics.averageSize = static_cast<double>(metrics.totalSize) / 
                            metrics.totalAllocations;
        metrics.stackTraces[record.stackHash]++;
    }

    void updateLifetimeStats(const AllocationRecord& record) {
        if (!record.isFreed) return;
        
        auto lifetime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - record.timestamp).count();
        
        metrics.lifetimes.push_back(lifetime);
        metrics.averageLifetimeMs = std::accumulate(
            metrics.lifetimes.begin(),
            metrics.lifetimes.end(), 0.0) / metrics.lifetimes.size();
    }

    std::vector<AllocationRecord> getExampleAllocations(size_t count) {
        std::vector<AllocationRecord> examples;
        for (const auto& [_, record] : allocations) {
            if (examples.size() >= count) break;
            examples.push_back(record);
        }
        return examples;
    }

    const char* patternTypeToString(PatternInfo::Type type) {
        switch (type) {
            case PatternInfo::Type::CYCLIC:
                return "Cyclic Pattern";
            case PatternInfo::Type::GROWING:
                return "Growing Memory Usage";
            case PatternInfo::Type::SPIKES:
                return "Memory Spikes";
            case PatternInfo::Type::FRAGMENTED:
                return "Memory Fragmentation";
            case PatternInfo::Type::LEAK_LIKELY:
                return "Potential Memory Leak";
            default:
                return "Normal Pattern";
        }
    }

    uint32_t captureStackHash() {
        // Simple stack hash calculation
        uint32_t hash = 0;
        #ifdef _WIN32
        void* stack[32];
        WORD frames = CaptureStackBackTrace(0, 32, stack, nullptr);
        for (WORD i = 0; i < frames; i++) {
            hash = hash * 31 + reinterpret_cast<uintptr_t>(stack[i]);
        }
        #else
        void* array[32];
        int frames = backtrace(array, 32);
        for (int i = 0; i < frames; i++) {
            hash = hash * 31 + reinterpret_cast<uintptr_t>(array[i]);
        }
        #endif
        return hash;
    }

    std::mutex mutex;
    std::map<void*, AllocationRecord> allocations;
    Metrics metrics;
};

// Macros for pattern analysis
#define ANALYZE_ALLOCATION_PATTERNS() \
    AllocationPatternAnalyzer::instance().analyzePatterns()

#define GENERATE_PATTERN_REPORT(filename) \
    AllocationPatternAnalyzer::instance().generateReport(filename)