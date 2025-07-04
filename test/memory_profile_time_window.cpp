#include "mesh/RadioInterface.h"
#include "plugins/TimeWindowPlugin.h"
#include "mesh/TimeWindow.h"
#include <memory>
#include <chrono>
#include <iomanip>
#include <fstream>

// Memory tracking
struct MemoryStats {
    size_t currentUsage = 0;
    size_t peakUsage = 0;
    size_t totalAllocations = 0;
    size_t totalDeallocations = 0;
    std::map<size_t, size_t> allocationSizes;  // size -> count
};

static MemoryStats memStats;

// Custom allocator for tracking
void* operator new(size_t size) {
    void* ptr = malloc(size);
    if (ptr) {
        memStats.currentUsage += size;
        memStats.peakUsage = std::max(memStats.peakUsage, memStats.currentUsage);
        memStats.totalAllocations++;
        memStats.allocationSizes[size]++;
    }
    return ptr;
}

void operator delete(void* ptr) noexcept {
    if (ptr) {
        memStats.totalDeallocations++;
        free(ptr);
    }
}

class MemoryProfiler {
public:
    static void resetStats() {
        memStats = MemoryStats();
    }

    static void reportStats(const std::string& testName) {
        std::ofstream report("memory_profile.csv", std::ios::app);
        report << std::fixed << std::setprecision(2);
        report << testName << ","
               << memStats.currentUsage << ","
               << memStats.peakUsage << ","
               << memStats.totalAllocations << ","
               << memStats.totalDeallocations << ","
               << getAverageAllocationSize() << "\n";
    }

private:
    static double getAverageAllocationSize() {
        size_t totalSize = 0;
        size_t count = 0;
        for (const auto& [size, num] : memStats.allocationSizes) {
            totalSize += size * num;
            count += num;
        }
        return count ? static_cast<double>(totalSize) / count : 0;
    }
};

// Test fixture
class MemoryProfileTest {
protected:
    std::unique_ptr<RadioInterface> radio;
    std::unique_ptr<TimeWindowPlugin> plugin;
    std::vector<meshtastic_MeshPacket*> testPackets;

    void setUp(size_t queueSize, size_t packetCount) {
        MemoryProfiler::resetStats();
        
        radio = std::make_unique<RadioInterface>();
        plugin = std::make_unique<TimeWindowPlugin>();

        // Configure time window
        config.has_lora = true;
        config.lora.time_window_enabled = true;
        config.lora.window_queue_size = queueSize;
        
        // Generate test packets
        generateTestPackets(packetCount);
    }

    void tearDown() {
        for (auto p : testPackets) {
            packetPool.release(p);
        }
        testPackets.clear();
        radio.reset();
        plugin.reset();
    }

private:
    void generateTestPackets(size_t count) {
        for (size_t i = 0; i < count; i++) {
            auto p = packetPool.allocZeroed();
            p->id = i;
            p->payload.size = 50;  // Fixed size for consistency
            testPackets.push_back(p);
        }
    }
};

// Memory profile tests
void profileQueueGrowth() {
    const std::vector<size_t> queueSizes = {32, 128, 512};
    const std::vector<size_t> packetCounts = {100, 1000, 5000};

    std::ofstream report("memory_profile.csv");
    report << "Test,Current Usage (B),Peak Usage (B),Allocations,Deallocations,Avg Allocation (B)\n";

    for (size_t queueSize : queueSizes) {
        for (size_t packetCount : packetCounts) {
            MemoryProfileTest test;
            test.setUp(queueSize, packetCount);

            std::string testName = "Queue_" + std::to_string(queueSize) + 
                                 "_Packets_" + std::to_string(packetCount);
            
            // Profile queue filling
            {
                auto start = std::chrono::steady_clock::now();
                for (auto p : test.testPackets) {
                    test.radio->send(p);
                }
                auto end = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                
                std::cout << testName << " Queue Fill Time: " 
                         << duration.count() << "us\n";
            }

            MemoryProfiler::reportStats(testName);
            test.tearDown();
        }
    }
}

void profileTimeWindowTransitions() {
    MemoryProfileTest test;
    test.setUp(128, 1000);

    const std::vector<std::pair<int, int>> times = {
        {9, 0},   // Window open
        {17, 0},  // Window close
        {12, 30}, // Mid-window
        {20, 0}   // Outside window
    };

    for (const auto& [hour, minute] : times) {
        std::string testName = "Transition_" + std::to_string(hour) + 
                             "_" + std::to_string(minute);
        
        MemoryProfiler::resetStats();
        
        // Simulate time change
        struct tm timeinfo = {};
        timeinfo.tm_hour = hour;
        timeinfo.tm_min = minute;
        time_t testTime = mktime(&timeinfo);
        setTime(testTime);

        // Process transitions
        test.plugin->runOnce();

        MemoryProfiler::reportStats(testName);
    }

    test.tearDown();
}

void profileQueueProcessing() {
    MemoryProfileTest test;
    test.setUp(128, 1000);

    // Fill queue
    for (auto p : test.testPackets) {
        test.radio->send(p);
    }

    MemoryProfiler::resetStats();
    
    // Process queue in chunks
    const std::vector<size_t> batchSizes = {10, 50, 100};
    
    for (size_t batchSize : batchSizes) {
        std::string testName = "Process_Batch_" + std::to_string(batchSize);
        
        auto start = std::chrono::steady_clock::now();
        size_t processed = 0;
        
        while (processed < test.testPackets.size()) {
            size_t count = std::min(batchSize, test.testPackets.size() - processed);
            for (size_t i = 0; i < count; i++) {
                test.plugin->runOnce();
            }
            processed += count;
        }
        
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << testName << " Processing Time: " 
                 << duration.count() << "us\n";
        
        MemoryProfiler::reportStats(testName);
    }

    test.tearDown();
}

int main() {
    std::cout << "Running memory profile tests...\n";
    
    profileQueueGrowth();
    profileTimeWindowTransitions();
    profileQueueProcessing();
    
    std::cout << "Memory profile results written to memory_profile.csv\n";
    return 0;
}