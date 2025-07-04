#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <deque>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "heap_guard.h"

/**
 * Memory access validator for detecting memory corruption and use-after-free
 */
class MemoryValidator {
public:
    struct CleanupEntry {
        void* ptr;
        std::chrono::steady_clock::time_point timestamp;
    };

    // Memory page states
    enum class PageState : uint8_t {
        UNALLOCATED = 0,
        ALLOCATED = 1,
        FREED = 2,
        GUARD = 3
    };

    // Page size for memory protection
    static constexpr size_t PAGE_SIZE = 4096;
    static constexpr size_t GUARD_PAGES = 1;

    static MemoryValidator& instance() {
        static MemoryValidator validator;
        return validator;
    }

    struct AccessViolation {
        enum class Type {
            USE_AFTER_FREE,
            BUFFER_OVERFLOW,
            BUFFER_UNDERFLOW,
            INVALID_ADDRESS
        };

        Type type;
        void* address;
        const char* operation;
        const char* file;
        int line;
    };

    void* validateAccess(void* ptr, size_t size, const char* operation, 
                        const char* file, int line) {
        if (!ptr) return nullptr;

        std::lock_guard<std::mutex> lock(mutex);
        
        auto it = allocations.find(ptr);
        if (it == allocations.end()) {
            reportViolation({
                AccessViolation::Type::INVALID_ADDRESS,
                ptr,
                operation,
                file,
                line
            });
            return nullptr;
        }

        AllocationInfo& info = it->second;
        if (info.state == PageState::FREED) {
            reportViolation({
                AccessViolation::Type::USE_AFTER_FREE,
                ptr,
                operation,
                file,
                line
            });
            return nullptr;
        }

        // Check for buffer overflow/underflow
        uint8_t* start = static_cast<uint8_t*>(ptr);
        uint8_t* end = start + size;

        if (start < info.start) {
            reportViolation({
                AccessViolation::Type::BUFFER_UNDERFLOW,
                ptr,
                operation,
                file,
                line
            });
            return nullptr;
        }

        if (end > info.end) {
            reportViolation({
                AccessViolation::Type::BUFFER_OVERFLOW,
                ptr,
                operation,
                file,
                line
            });
            return nullptr;
        }

        return ptr;
    }

    void trackAllocation(void* ptr, size_t size) {
        if (!ptr) return;

        std::lock_guard<std::mutex> lock(mutex);
        
        // Align to page boundaries
        uint8_t* start = static_cast<uint8_t*>(ptr);
        uint8_t* userStart = start + (GUARD_PAGES * PAGE_SIZE);
        uint8_t* userEnd = userStart + size;
        uint8_t* end = userEnd + (GUARD_PAGES * PAGE_SIZE);

        // Setup guard pages
        protectRange(start, PAGE_SIZE, PageState::GUARD);
        protectRange(userEnd, PAGE_SIZE, PageState::GUARD);

        // Track allocation
        allocations[ptr] = {
            PageState::ALLOCATED,
            start,
            end,
            size
        };

        totalAllocated += size;
        activeAllocations++;
    }

    void trackDeallocation(void* ptr) {
        if (!ptr) return;

        std::lock_guard<std::mutex> lock(mutex);
        
        auto it = allocations.find(ptr);
        if (it != allocations.end()) {
            AllocationInfo& info = it->second;
            
            // Mark as freed but keep guard pages
            info.state = PageState::FREED;
            protectRange(info.start, info.size, PageState::FREED);

            totalFreed += info.size;
            activeAllocations--;

            // Schedule for cleanup after grace period
            cleanupQueue.push_back({ptr, std::chrono::steady_clock::now()});
            processCleanupQueue();
        }
    }

    struct Statistics {
        size_t totalAllocated;
        size_t totalFreed;
        size_t activeAllocations;
        size_t violationCount;
    };

    Statistics getStatistics() const {
        return {
            totalAllocated,
            totalFreed,
            activeAllocations,
            violationCount
        };
    }

private:
    struct AllocationInfo {
        PageState state;
        uint8_t* start;
        uint8_t* end;
        size_t size;
    };

    MemoryValidator() 
        : totalAllocated(0), totalFreed(0), 
          activeAllocations(0), violationCount(0) {}

    void protectRange(void* addr, size_t size, PageState state) {
        #ifdef _WIN32
        DWORD oldProtect;
        DWORD newProtect = (state == PageState::GUARD) ? 
            PAGE_NOACCESS : PAGE_READWRITE;
        VirtualProtect(addr, size, newProtect, &oldProtect);
        #else
        int prot = (state == PageState::GUARD) ? 
            PROT_NONE : (PROT_READ | PROT_WRITE);
        mprotect(addr, size, prot);
        #endif
    }

    void processCleanupQueue() {
        auto now = std::chrono::steady_clock::now();
        while (!cleanupQueue.empty()) {
            const auto& entry = cleanupQueue.front();
            if (now - entry.timestamp < CLEANUP_DELAY) {
                break;
            }

            auto it = allocations.find(entry.ptr);
            if (it != allocations.end()) {
                protectRange(it->second.start, 
                           it->second.end - it->second.start, 
                           PageState::UNALLOCATED);
                allocations.erase(it);
            }

            cleanupQueue.pop_front();
        }
    }

    void reportViolation(const AccessViolation& violation) {
        violationCount++;
        
        char buffer[1024];
        snprintf(buffer, sizeof(buffer),
            "Memory access violation: %s at %p\n"
            "Operation: %s\n"
            "Location: %s:%d\n",
            violationTypeToString(violation.type),
            violation.address,
            violation.operation,
            violation.file,
            violation.line);
            
        fprintf(stderr, "%s", buffer);

        #ifdef _WIN32
        if (IsDebuggerPresent()) {
            DebugBreak();
        }
        #endif
    }

    const char* violationTypeToString(AccessViolation::Type type) {
        switch (type) {
            case AccessViolation::Type::USE_AFTER_FREE:
                return "Use after free";
            case AccessViolation::Type::BUFFER_OVERFLOW:
                return "Buffer overflow";
            case AccessViolation::Type::BUFFER_UNDERFLOW:
                return "Buffer underflow";
            case AccessViolation::Type::INVALID_ADDRESS:
                return "Invalid address";
            default:
                return "Unknown violation";
        }
    }

    static constexpr auto CLEANUP_DELAY = std::chrono::seconds(30);

    std::mutex mutex;
    std::map<void*, AllocationInfo> allocations;
    std::deque<CleanupEntry> cleanupQueue;
    
    std::atomic<size_t> totalAllocated;
    std::atomic<size_t> totalFreed;
    std::atomic<size_t> activeAllocations;
    std::atomic<size_t> violationCount;
};

// Macros for memory validation
#ifdef ENABLE_MEMORY_VALIDATION
    #define VALIDATE_READ(ptr, size) \
        MemoryValidator::instance().validateAccess(ptr, size, "read", __FILE__, __LINE__)
    #define VALIDATE_WRITE(ptr, size) \
        MemoryValidator::instance().validateAccess(ptr, size, "write", __FILE__, __LINE__)
#else
    #define VALIDATE_READ(ptr, size) (ptr)
    #define VALIDATE_WRITE(ptr, size) (ptr)
#endif