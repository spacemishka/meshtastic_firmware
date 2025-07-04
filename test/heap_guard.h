#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <stdexcept>
#include <string>
#include <sstream>

#ifdef _WIN32
#include <Windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")
#else
#include <execinfo.h>
#include <cxxabi.h>
#endif

/**
 * HeapGuard provides heap corruption detection for time window testing
 */
class HeapGuard {
public:
    // Magic values for heap guard detection
    static constexpr uint32_t GUARD_PATTERN_HEAD = 0xDEADBEEF;
    static constexpr uint32_t GUARD_PATTERN_TAIL = 0xBEEFDEAD;
    static constexpr size_t GUARD_SIZE = sizeof(uint32_t);
    
    struct BlockHeader {
        uint32_t guardHead;
        size_t size;
        const char* file;
        int line;
        std::atomic<bool> isFreed;
        uint32_t checksum;
        char stackTrace[1024];  // Fixed-size buffer for stack trace
    };

    struct BlockFooter {
        uint32_t guardTail;
    };

    static HeapGuard& instance() {
        static HeapGuard guard;
        return guard;
    }

    void* allocateGuarded(size_t size, const char* file, int line) {
        // Calculate total size including guards and metadata
        size_t totalSize = sizeof(BlockHeader) + size + sizeof(BlockFooter);
        
        // Allocate memory block
        uint8_t* block = static_cast<uint8_t*>(std::malloc(totalSize));
        if (!block) {
            throw std::bad_alloc();
        }
        
        // Setup header
        BlockHeader* header = new(block) BlockHeader{
            GUARD_PATTERN_HEAD,
            size,
            file,
            line,
            false,
            0,
            {0}  // Initialize stack trace buffer
        };
        
        // Capture stack trace if enabled
        if (captureStackTraces) {
            captureStackTrace(header->stackTrace, sizeof(header->stackTrace));
        }
        
        // Setup user memory area
        uint8_t* userData = block + sizeof(BlockHeader);
        std::memset(userData, 0xCD, size); // Fill with debug pattern
        
        // Setup footer
        BlockFooter* footer = reinterpret_cast<BlockFooter*>(userData + size);
        footer->guardTail = GUARD_PATTERN_TAIL;
        
        // Calculate checksum
        header->checksum = calculateChecksum(block, totalSize);
        
        return userData;
    }

    void deallocateGuarded(void* ptr) {
        if (!ptr) return;
        
        try {
            // Get block header
            uint8_t* userData = static_cast<uint8_t*>(ptr);
            BlockHeader* header = reinterpret_cast<BlockHeader*>(userData - sizeof(BlockHeader));
            
            // Check for double-free
            bool wasFreed = header->isFreed.exchange(true);
            if (wasFreed) {
                throwError("Double free detected", header);
            }
            
            // Verify header guard
            if (header->guardHead != GUARD_PATTERN_HEAD) {
                throwError("Heap corruption detected: Header guard pattern invalid", header);
            }
            
            // Verify footer guard
            BlockFooter* footer = reinterpret_cast<BlockFooter*>(userData + header->size);
            if (footer->guardTail != GUARD_PATTERN_TAIL) {
                throwError("Heap corruption detected: Footer guard pattern invalid", header);
            }
            
            // Verify checksum
            size_t totalSize = sizeof(BlockHeader) + header->size + sizeof(BlockFooter);
            uint32_t currentChecksum = calculateChecksum(reinterpret_cast<uint8_t*>(header), totalSize);
            if (currentChecksum != header->checksum) {
                throwError("Heap corruption detected: Block checksum mismatch", header);
            }
            
            // Fill memory with debug pattern
            std::memset(userData, 0xDD, header->size);
            
            // Free the block
            std::free(header);
        } catch (const std::exception& e) {
            // Log error and continue, as we can't throw from delete
            fprintf(stderr, "Error in deallocateGuarded: %s\n", e.what());
        }
    }

    void validateBlock(void* ptr) {
        if (!ptr) return;
        
        uint8_t* userData = static_cast<uint8_t*>(ptr);
        BlockHeader* header = reinterpret_cast<BlockHeader*>(userData - sizeof(BlockHeader));
        
        if (header->guardHead != GUARD_PATTERN_HEAD) {
            throwError("Heap corruption detected: Header guard pattern invalid", header);
        }
        
        BlockFooter* footer = reinterpret_cast<BlockFooter*>(userData + header->size);
        if (footer->guardTail != GUARD_PATTERN_TAIL) {
            throwError("Heap corruption detected: Footer guard pattern invalid", header);
        }
        
        size_t totalSize = sizeof(BlockHeader) + header->size + sizeof(BlockFooter);
        uint32_t currentChecksum = calculateChecksum(reinterpret_cast<uint8_t*>(header), totalSize);
        if (currentChecksum != header->checksum) {
            throwError("Heap corruption detected: Block checksum mismatch", header);
        }
    }

    void enableStackTrace(bool enable) {
        captureStackTraces = enable;
    }

private:
    HeapGuard() : captureStackTraces(false) {}

    uint32_t calculateChecksum(uint8_t* block, size_t size) {
        uint32_t checksum = 0;
        BlockHeader* header = reinterpret_cast<BlockHeader*>(block);
        
        // Save and clear existing checksum for calculation
        uint32_t savedChecksum = header->checksum;
        header->checksum = 0;
        
        // Calculate new checksum
        for (size_t i = 0; i < size; i++) {
            checksum = rotateLeft(checksum, 1) + block[i];
        }
        
        // Restore original checksum
        header->checksum = savedChecksum;
        return checksum;
    }

    uint32_t rotateLeft(uint32_t value, unsigned int bits) {
        return (value << bits) | (value >> (32 - bits));
    }

    void throwError(const char* message, BlockHeader* header) {
        char buffer[2048];
        snprintf(buffer, sizeof(buffer),
            "%s\nBlock allocated at %s:%d\nSize: %zu bytes\n%s%s",
            message,
            header->file,
            header->line,
            header->size,
            captureStackTraces ? "Stack trace at allocation:\n" : "",
            captureStackTraces ? header->stackTrace : "");
            
        throw std::runtime_error(buffer);
    }

    void captureStackTrace(char* buffer, size_t bufferSize) {
        #ifdef _WIN32
        void* stack[32];
        WORD frames = CaptureStackBackTrace(0, 32, stack, nullptr);
        
        size_t offset = 0;
        for (WORD i = 0; i < frames && offset < bufferSize - 100; i++) {
            SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(
                calloc(sizeof(SYMBOL_INFO) + 256, 1));
            symbol->MaxNameLen = 255;
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            
            if (SymFromAddr(GetCurrentProcess(), 
                reinterpret_cast<DWORD64>(stack[i]), 0, symbol)) {
                offset += snprintf(buffer + offset, bufferSize - offset,
                    "\t%s\n", symbol->Name);
            }
            free(symbol);
        }
        #else
        void* array[32];
        int frames = backtrace(array, 32);
        char** strings = backtrace_symbols(array, frames);
        
        if (strings) {
            size_t offset = 0;
            for (int i = 0; i < frames && offset < bufferSize - 100; i++) {
                offset += snprintf(buffer + offset, bufferSize - offset,
                    "\t%s\n", strings[i]);
            }
            free(strings);
        }
        #endif
    }

    bool captureStackTraces;
};

// Operator overloads for guarded allocation
void* operator new(size_t size, const char* file, int line) {
    return HeapGuard::instance().allocateGuarded(size, file, line);
}

void operator delete(void* ptr) noexcept {
    HeapGuard::instance().deallocateGuarded(ptr);
}

// Macros for heap guard usage
#ifdef ENABLE_HEAP_GUARD
    #define new new(__FILE__, __LINE__)
    #define VALIDATE_HEAP_BLOCK(ptr) HeapGuard::instance().validateBlock(ptr)
    #define ENABLE_HEAP_STACK_TRACE() HeapGuard::instance().enableStackTrace(true)
#else
    #define VALIDATE_HEAP_BLOCK(ptr)
    #define ENABLE_HEAP_STACK_TRACE()
#endif