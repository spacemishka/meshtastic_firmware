#pragma once

#include <string>
#include <sstream>
#include <fstream>
#include <chrono>
#include <mutex>
#include <memory>
#include <vector>
#include <filesystem>
#include <iomanip>
#include <ctime>
#include <queue>

namespace meshtastic {
namespace test {

/**
 * Test execution logging system
 */
class TestLogger {
public:
    enum class Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };

    struct LogConfig {
        std::string logDir = "test_logs";
        std::string filename = "test_execution.log";
        Level minLevel = Level::INFO;
        bool consoleOutput = true;
        bool fileOutput = true;
        bool includeTimestamp = true;
        bool includeLine = true;
        size_t maxFileSize = 10 * 1024 * 1024; // 10MB
        size_t maxFiles = 5;
        size_t bufferSize = 1000;
    };

    static TestLogger& instance() {
        static TestLogger logger;
        return logger;
    }

    void configure(const LogConfig& config) {
        const std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        setupLogFile();
    }

    void debug(const std::string& message, const char* file = nullptr, int line = 0) {
        log(Level::DEBUG, message, file, line);
    }

    void info(const std::string& message, const char* file = nullptr, int line = 0) {
        log(Level::INFO, message, file, line);
    }

    void warning(const std::string& message, const char* file = nullptr, int line = 0) {
        log(Level::WARNING, message, file, line);
    }

    void error(const std::string& message, const char* file = nullptr, int line = 0) {
        log(Level::ERROR, message, file, line);
    }

    void critical(const std::string& message, const char* file = nullptr, int line = 0) {
        log(Level::CRITICAL, message, file, line);
    }

    void flush() {
        const std::lock_guard<std::mutex> lock(mutex_);
        flushBuffer();
    }

    std::vector<std::string> getRecentLogs(size_t count = 100) const {
        const std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> logs;
        size_t start = (logBuffer_.size() > count) ? logBuffer_.size() - count : 0;
        for (size_t i = start; i < logBuffer_.size(); i++) {
            logs.push_back(logBuffer_[i]);
        }
        return logs;
    }

    void clearLogs() {
        const std::lock_guard<std::mutex> lock(mutex_);
        logBuffer_.clear();
        if (logFile_.is_open()) {
            logFile_.close();
            logFile_.open(getLogPath(), std::ios::out | std::ios::trunc);
        }
    }

private:
    mutable std::mutex mutex_;
    LogConfig config_;
    std::ofstream logFile_;
    std::vector<std::string> logBuffer_;
    size_t currentFileSize_ = 0;

    void log(Level level, const std::string& message, const char* file, int line) {
        if (level < config_.minLevel) return;

        std::stringstream ss;
        
        // Add timestamp
        if (config_.includeTimestamp) {
            ss << "[" << getCurrentTimestamp() << "] ";
        }

        // Add level
        ss << "[" << levelToString(level) << "] ";

        // Add file and line
        if (config_.includeLine && file != nullptr) {
            ss << "[" << std::filesystem::path(file).filename().string() 
               << ":" << line << "] ";
        }

        // Add message
        ss << message;
        std::string logEntry = ss.str();

        // Console output
        if (config_.consoleOutput) {
            std::cout << logEntry << std::endl;
        }

        const std::lock_guard<std::mutex> lock(mutex_);

        // Buffer management
        logBuffer_.push_back(logEntry);
        if (logBuffer_.size() > config_.bufferSize) {
            logBuffer_.erase(logBuffer_.begin());
        }

        // File output
        if (config_.fileOutput) {
            writeToFile(logEntry);
        }
    }

    void writeToFile(const std::string& entry) {
        if (!logFile_.is_open()) {
            setupLogFile();
        }

        logFile_ << entry << std::endl;
        currentFileSize_ += entry.size() + 1;

        if (currentFileSize_ >= config_.maxFileSize) {
            rotateLogFiles();
        }
    }

    void setupLogFile() {
        std::filesystem::create_directories(config_.logDir);
        logFile_.open(getLogPath(), std::ios::app);
        if (logFile_.is_open()) {
            logFile_.seekp(0, std::ios::end);
            currentFileSize_ = logFile_.tellp();
        }
    }

    void rotateLogFiles() {
        logFile_.close();

        // Rotate existing files
        for (int i = config_.maxFiles - 1; i >= 0; --i) {
            std::filesystem::path current = getLogPath(i);
            std::filesystem::path next = getLogPath(i + 1);

            if (std::filesystem::exists(current)) {
                if (i == static_cast<int>(config_.maxFiles) - 1) {
                    std::filesystem::remove(current);
                } else {
                    std::filesystem::rename(current, next);
                }
            }
        }

        // Create new log file
        setupLogFile();
    }

    std::string getCurrentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    std::filesystem::path getLogPath(int index = 0) const {
        if (index == 0) {
            return std::filesystem::path(config_.logDir) / config_.filename;
        }
        std::string basename = config_.filename;
        size_t ext = basename.find_last_of('.');
        if (ext != std::string::npos) {
            basename.insert(ext, "." + std::to_string(index));
        } else {
            basename += "." + std::to_string(index);
        }
        return std::filesystem::path(config_.logDir) / basename;
    }

    const char* levelToString(Level level) const {
        switch (level) {
            case Level::DEBUG: return "DEBUG";
            case Level::INFO: return "INFO";
            case Level::WARNING: return "WARN";
            case Level::ERROR: return "ERROR";
            case Level::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }

    void flushBuffer() {
        if (config_.fileOutput && logFile_.is_open()) {
            logFile_.flush();
        }
    }
};

} // namespace test
} // namespace meshtastic

// Helper macros for logging
#define LOG_DEBUG(message) \
    meshtastic::test::TestLogger::instance().debug(message, __FILE__, __LINE__)

#define LOG_INFO(message) \
    meshtastic::test::TestLogger::instance().info(message, __FILE__, __LINE__)

#define LOG_WARNING(message) \
    meshtastic::test::TestLogger::instance().warning(message, __FILE__, __LINE__)

#define LOG_ERROR(message) \
    meshtastic::test::TestLogger::instance().error(message, __FILE__, __LINE__)

#define LOG_CRITICAL(message) \
    meshtastic::test::TestLogger::instance().critical(message, __FILE__, __LINE__)

#define CONFIGURE_LOGGER(config) \
    meshtastic::test::TestLogger::instance().configure(config)

#define FLUSH_LOGS() \
    meshtastic::test::TestLogger::instance().flush()

#define CLEAR_LOGS() \
    meshtastic::test::TestLogger::instance().clearLogs()

#define GET_RECENT_LOGS(count) \
    meshtastic::test::TestLogger::instance().getRecentLogs(count)