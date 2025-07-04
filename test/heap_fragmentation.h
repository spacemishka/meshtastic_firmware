#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <numeric>
#include "memory_validator.h"

/**
 * Heap fragmentation analyzer for detecting memory allocation patterns
 */
class HeapFragmentationAnalyzer {
public:
    struct FragmentationInfo {
        size_t totalHeapSize;
        size_t usedMemory;
        size_t largestFreeBlock;
        size_t totalFragments;
        double fragmentationIndex;  // 0 (best) to 1 (worst)
        std::vector<size_t> freeBlockSizes;
        std::vector<size_t> usedBlockSizes;
    };

    struct BlockInfo {
        uintptr_t address;
        size_t size;
        bool isUsed;
    };

    static HeapFragmentationAnalyzer& instance() {
        static HeapFragmentationAnalyzer analyzer;
        return analyzer;
    }

    FragmentationInfo analyze() {
        std::lock_guard<std::mutex> lock(mutex);
        
        // Get all memory blocks
        auto blocks = collectMemoryBlocks();
        
        // Sort blocks by address
        std::sort(blocks.begin(), blocks.end(),
            [](const BlockInfo& a, const BlockInfo& b) {
                return a.address < b.address;
            });

        // Calculate metrics
        FragmentationInfo info;
        info.totalHeapSize = calculateTotalHeapSize(blocks);
        info.usedMemory = calculateUsedMemory(blocks);
        info.totalFragments = countFragments(blocks);
        info.largestFreeBlock = findLargestFreeBlock(blocks);
        
        // Collect block size distributions
        collectBlockSizes(blocks, info);
        
        // Calculate fragmentation index
        info.fragmentationIndex = calculateFragmentationIndex(info);
        
        return info;
    }

    void generateReport(const char* filename) {
        auto info = analyze();
        
        FILE* f = fopen(filename, "w");
        if (!f) return;

        fprintf(f, "=== Heap Fragmentation Report ===\n\n");
        fprintf(f, "Total Heap Size: %zu bytes\n", info.totalHeapSize);
        fprintf(f, "Used Memory: %zu bytes (%.1f%%)\n", 
                info.usedMemory, 
                (double)info.usedMemory * 100 / info.totalHeapSize);
        fprintf(f, "Largest Free Block: %zu bytes\n", info.largestFreeBlock);
        fprintf(f, "Total Fragments: %zu\n", info.totalFragments);
        fprintf(f, "Fragmentation Index: %.3f\n\n", info.fragmentationIndex);

        fprintf(f, "Free Block Distribution:\n");
        printSizeDistribution(f, info.freeBlockSizes);
        
        fprintf(f, "\nUsed Block Distribution:\n");
        printSizeDistribution(f, info.usedBlockSizes);

        if (info.fragmentationIndex > 0.7) {
            fprintf(f, "\nWARNING: High fragmentation detected!\n");
            fprintf(f, "Consider implementing defragmentation or reviewing allocation patterns.\n");
        }

        fclose(f);
    }

    bool isHighlyFragmented() const {
        return analyze().fragmentationIndex > 0.7;
    }

    double getFragmentationIndex() const {
        return analyze().fragmentationIndex;
    }

private:
    HeapFragmentationAnalyzer() {}

    std::vector<BlockInfo> collectMemoryBlocks() {
        std::vector<BlockInfo> blocks;
        
        // Get allocated blocks from memory validator
        auto& validator = MemoryValidator::instance();
        const auto& allocations = validator.getAllocations();
        
        for (const auto& alloc : allocations) {
            blocks.push_back({
                reinterpret_cast<uintptr_t>(alloc.first),
                alloc.second.size,
                true
            });
        }

        // Add gaps as free blocks
        if (!blocks.empty()) {
            for (size_t i = 1; i < blocks.size(); i++) {
                uintptr_t prevEnd = blocks[i-1].address + blocks[i-1].size;
                uintptr_t gap = blocks[i].address - prevEnd;
                
                if (gap > 0) {
                    blocks.push_back({
                        prevEnd,
                        gap,
                        false
                    });
                }
            }
        }

        return blocks;
    }

    size_t calculateTotalHeapSize(const std::vector<BlockInfo>& blocks) {
        if (blocks.empty()) return 0;
        
        auto firstBlock = std::min_element(blocks.begin(), blocks.end(),
            [](const BlockInfo& a, const BlockInfo& b) {
                return a.address < b.address;
            });
            
        auto lastBlock = std::max_element(blocks.begin(), blocks.end(),
            [](const BlockInfo& a, const BlockInfo& b) {
                return (a.address + a.size) < (b.address + b.size);
            });
            
        return (lastBlock->address + lastBlock->size) - firstBlock->address;
    }

    size_t calculateUsedMemory(const std::vector<BlockInfo>& blocks) {
        return std::accumulate(blocks.begin(), blocks.end(), 0ULL,
            [](size_t sum, const BlockInfo& block) {
                return sum + (block.isUsed ? block.size : 0);
            });
    }

    size_t countFragments(const std::vector<BlockInfo>& blocks) {
        size_t count = 0;
        bool inFreeBlock = false;
        
        for (const auto& block : blocks) {
            if (!block.isUsed) {
                if (!inFreeBlock) {
                    count++;
                    inFreeBlock = true;
                }
            } else {
                inFreeBlock = false;
            }
        }
        
        return count;
    }

    size_t findLargestFreeBlock(const std::vector<BlockInfo>& blocks) {
        size_t largest = 0;
        
        for (const auto& block : blocks) {
            if (!block.isUsed) {
                largest = std::max(largest, block.size);
            }
        }
        
        return largest;
    }

    void collectBlockSizes(const std::vector<BlockInfo>& blocks, 
                          FragmentationInfo& info) {
        for (const auto& block : blocks) {
            if (block.isUsed) {
                info.usedBlockSizes.push_back(block.size);
            } else {
                info.freeBlockSizes.push_back(block.size);
            }
        }
        
        std::sort(info.freeBlockSizes.begin(), info.freeBlockSizes.end());
        std::sort(info.usedBlockSizes.begin(), info.usedBlockSizes.end());
    }

    double calculateFragmentationIndex(const FragmentationInfo& info) {
        if (info.totalHeapSize == 0) return 0.0;
        
        // Factors that contribute to fragmentation:
        // 1. Number of fragments relative to total blocks
        // 2. Size of largest free block relative to total free space
        // 3. Distribution of free block sizes
        
        double fragmentCount = static_cast<double>(info.totalFragments) / 
                             (info.freeBlockSizes.size() + info.usedBlockSizes.size());
                             
        double freeSpaceUtilization = 1.0 - (static_cast<double>(info.largestFreeBlock) / 
                                    (info.totalHeapSize - info.usedMemory));
                                    
        double sizeVariation = calculateSizeVariation(info.freeBlockSizes);
        
        // Weighted combination of factors
        return (0.4 * fragmentCount + 
                0.4 * freeSpaceUtilization + 
                0.2 * sizeVariation);
    }

    double calculateSizeVariation(const std::vector<size_t>& sizes) {
        if (sizes.empty()) return 0.0;
        
        // Calculate mean
        double mean = std::accumulate(sizes.begin(), sizes.end(), 0.0) / sizes.size();
        
        // Calculate variance
        double variance = std::accumulate(sizes.begin(), sizes.end(), 0.0,
            [mean](double acc, size_t size) {
                double diff = size - mean;
                return acc + (diff * diff);
            }) / sizes.size();
            
        // Return coefficient of variation
        return std::sqrt(variance) / mean;
    }

    void printSizeDistribution(FILE* f, const std::vector<size_t>& sizes) {
        if (sizes.empty()) {
            fprintf(f, "  No blocks\n");
            return;
        }

        // Calculate distribution buckets
        std::vector<size_t> buckets(10, 0);
        size_t maxSize = *std::max_element(sizes.begin(), sizes.end());
        
        for (size_t size : sizes) {
            size_t bucket = (size * 9) / maxSize;
            buckets[bucket]++;
        }

        // Print histogram
        for (size_t i = 0; i < buckets.size(); i++) {
            size_t start = (i * maxSize) / 9;
            size_t end = ((i + 1) * maxSize) / 9;
            fprintf(f, "  %6zu - %6zu bytes: %zu blocks |", 
                    start, end, buckets[i]);
            
            // Print bar
            size_t barLength = (buckets[i] * 50) / 
                             *std::max_element(buckets.begin(), buckets.end());
            fprintf(f, "%.*s\n", (int)barLength, "=====================================");
        }
    }

    std::mutex mutex;
};

// Macros for heap fragmentation analysis
#define ANALYZE_HEAP_FRAGMENTATION() \
    HeapFragmentationAnalyzer::instance().analyze()

#define GENERATE_FRAGMENTATION_REPORT(filename) \
    HeapFragmentationAnalyzer::instance().generateReport(filename)

#define CHECK_FRAGMENTATION() \
    HeapFragmentationAnalyzer::instance().isHighlyFragmented()