#pragma once

#include <cstddef>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <iostream>
#include <memory>

#ifdef _WIN32
#include <Windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")
#else
#include <execinfo.h>
#include <cxxabi.h>
#endif

/**
 * Memory leak detection system for time window testing
 */
class MemoryLeakDetector {
public:
    struct AllocationInfo {
        size_t size;
        const char* file;
        int line;
        std::string stackTrace;
        void* addr;
    };

    static MemoryLeakDetector& instance() {
        static MemoryLeakDetector detector;
        return detector;
    }

    void startTracking() {
        std::lock_guard<std::mutex> lock(mutex);
        isTracking = true;
        allocations.clear();
        totalAllocated = 0;
        totalFreed = 0;
    }

    void stopTracking() {
        std::lock_guard<std::mutex> lock(mutex);
        isTracking = false;
    }

    void recordAllocation(void* ptr, size_t size, const char* file, int line) {
        if (!isTracking) return;
        
        std::lock_guard<std::mutex> lock(mutex);
        
        AllocationInfo info;
        info.size = size;
        info.file = file;
        info.line = line;
        info.addr = ptr;
        info.stackTrace = captureStackTrace();
        
        allocations[ptr] = info;
        totalAllocated += size;
    }

    void recordDeallocation(void* ptr) {
        if (!isTracking) return;
        
        std::lock_guard<std::mutex> lock(mutex);
        
        auto it = allocations.find(ptr);
        if (it != allocations.end()) {
            totalFreed += it->second.size;
            allocations.erase(it);
        }
    }

    std::string generateReport() {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::stringstream report;
        report << "=== Memory Leak Report ===\n\n";
        report << "Total allocated: " << totalAllocated << " bytes\n";
        report << "Total freed: " << totalFreed << " bytes\n";
        report << "Potential leaks: " << (totalAllocated - totalFreed) << " bytes\n\n";

        if (!allocations.empty()) {
            report << "Unfreed allocations:\n";
            for (const auto& [ptr, info] : allocations) {
                report << "Address: " << ptr << "\n";
                report << "Size: " << info.size << " bytes\n";
                report << "Location: " << info.file << ":" << info.line << "\n";
                report << "Stack trace:\n" << info.stackTrace << "\n\n";
            }
        }

        return report.str();
    }

    void validateNoLeaks() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!allocations.empty()) {
            std::stringstream ss;
            ss << "Memory leaks detected: " << allocations.size() << " unfreed allocations";
            throw std::runtime_error(ss.str());
        }
    }

    void dumpLeakInfo(const std::string& filename) {
        std::string report = generateReport();
        std::ofstream out(filename.c_str(), std::ios::out | std::ios::trunc);
        if (out) {
            out.write(report.c_str(), report.size());
            out.close();
        }
    }

    struct AllocationPattern {
        size_t count;
        size_t totalSize;
        std::vector<AllocationInfo> examples;
    };

    std::map<std::string, AllocationPattern> analyzePatterns() {
        std::lock_guard<std::mutex> lock(mutex);
        std::map<std::string, AllocationPattern> patterns;

        for (const auto& [ptr, info] : allocations) {
            std::string key = std::string(info.file) + ":" + std::to_string(info.line);
            auto& pattern = patterns[key];
            pattern.count++;
            pattern.totalSize += info.size;
            if (pattern.examples.size() < 3) {
                pattern.examples.push_back(info);
            }
        }

        return patterns;
    }

private:
    MemoryLeakDetector() : isTracking(false), totalAllocated(0), totalFreed(0) {
        #ifdef _WIN32
        SymInitialize(GetCurrentProcess(), NULL, TRUE);
        #endif
    }
    
    std::string captureStackTrace() {
        std::stringstream trace;
        
        #ifdef _WIN32
        const int MAX_FRAMES = 32;
        void* stack[MAX_FRAMES];
        WORD frames = CaptureStackBackTrace(0, MAX_FRAMES, stack, nullptr);
        
        std::unique_ptr<SYMBOL_INFO> symbol(
            reinterpret_cast<SYMBOL_INFO*>(calloc(sizeof(SYMBOL_INFO) + MAX_NAME_LEN, 1)));
        symbol->MaxNameLen = MAX_NAME_LEN;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

        for (WORD i = 0; i < frames; i++) {
            if (SymFromAddr(GetCurrentProcess(), 
                reinterpret_cast<DWORD64>(stack[i]), 0, symbol.get())) {
                trace << "\t" << symbol->Name << "\n";
            }
        }
        #else
        const int MAX_FRAMES = 32;
        void* array[MAX_FRAMES];
        int size = backtrace(array, MAX_FRAMES);
        char** strings = backtrace_symbols(array, size);

        if (strings) {
            for (int i = 0; i < size; i++) {
                char* begin = nullptr;
                char* end = nullptr;
                
                // Find parentheses and +address offset surrounding mangled name
                for (char* j = strings[i]; *j; ++j) {
                    if (*j == '(') begin = j;
                    else if (*j == '+') end = j;
                }
                
                if (begin && end) {
                    *end = '\0';  // Terminate string at offset
                    begin++;      // Skip (
                    
                    int status;
                    char* demangled = abi::__cxa_demangle(begin, nullptr, nullptr, &status);
                    if (demangled) {
                        trace << "\t" << demangled << "\n";
                        free(demangled);
                    } else {
                        trace << "\t" << begin << "\n";
                    }
                } else {
                    trace << "\t" << strings[i] << "\n";
                }
            }
            free(strings);
        }
        #endif
        
        return trace.str();
    }

    static constexpr size_t MAX_NAME_LEN = 256;
    std::mutex mutex;
    bool isTracking;
    std::map<void*, AllocationInfo> allocations;
    size_t totalAllocated;
    size_t totalFreed;
};

#ifdef TRACK_MEMORY_LEAKS
void* operator new(size_t size, const char* file, int line) {
    void* ptr = std::malloc(size);
    if (ptr) {
        MemoryLeakDetector::instance().recordAllocation(ptr, size, file, line);
    }
    return ptr;
}

void operator delete(void* ptr) noexcept {
    MemoryLeakDetector::instance().recordDeallocation(ptr);
    std::free(ptr);
}

#define new new(__FILE__, __LINE__)
#endif

class ScopedMemoryLeakDetection {
public:
    explicit ScopedMemoryLeakDetection(const char* scope_name = nullptr) : name(scope_name) {
        MemoryLeakDetector::instance().startTracking();
    }
    
    ~ScopedMemoryLeakDetection() {
        MemoryLeakDetector::instance().stopTracking();
        try {
            MemoryLeakDetector::instance().validateNoLeaks();
        } catch (const std::exception& e) {
            if (name) {
                fprintf(stderr, "Memory leak detected in scope: %s\n", name);
            }
            fprintf(stderr, "%s\n", e.what());
            MemoryLeakDetector::instance().dumpLeakInfo("memory_leaks.txt");
        }
    }

private:
    const char* name;
};

#define DETECT_LEAKS(name) ScopedMemoryLeakDetection leak_detector(name)