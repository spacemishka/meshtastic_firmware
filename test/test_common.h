#pragma once

#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace meshtastic {
namespace test {

/**
 * Common utilities for test components
 */
class TestCommon {
public:
    enum class LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };

    static std::string levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }

    static LogLevel stringToLevel(const std::string& level) {
        if (level == "DEBUG") return LogLevel::DEBUG;
        if (level == "INFO") return LogLevel::INFO;
        if (level == "WARN") return LogLevel::WARNING;
        if (level == "ERROR") return LogLevel::ERROR;
        if (level == "CRITICAL") return LogLevel::CRITICAL;
        return LogLevel::INFO;
    }

    static std::string formatTimestamp(const std::chrono::system_clock::time_point& tp) {
        auto time = std::chrono::system_clock::to_time_t(tp);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            tp.time_since_epoch()) % 1000;
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    static std::string formatShortTimestamp(const std::chrono::system_clock::time_point& tp) {
        auto time = std::chrono::system_clock::to_time_t(tp);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%H:%M:%S");
        return ss.str();
    }

    static std::string formatDuration(std::chrono::milliseconds duration) {
        auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
        duration -= hours;
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
        duration -= minutes;
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
        duration -= seconds;
        
        std::stringstream ss;
        if (hours.count() > 0) {
            ss << hours.count() << "h ";
        }
        if (minutes.count() > 0 || hours.count() > 0) {
            ss << minutes.count() << "m ";
        }
        ss << seconds.count() << "." 
           << std::setw(3) << std::setfill('0') << duration.count() << "s";
        return ss.str();
    }

    static std::string formatBytes(size_t bytes) {
        static const char* units[] = {"B", "KB", "MB", "GB"};
        int unit = 0;
        double size = static_cast<double>(bytes);
        while (size >= 1024.0 && unit < 3) {
            size /= 1024.0;
            unit++;
        }
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << size << " " << units[unit];
        return ss.str();
    }

    static std::chrono::system_clock::time_point parseTimestamp(const std::string& ts) {
        std::tm tm = {};
        std::istringstream ss(ts);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (ss.fail()) {
            return std::chrono::system_clock::now();
        }
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    static std::string escapeCsv(const std::string& str) {
        if (str.find_first_of(",\"\n") == std::string::npos) {
            return str;
        }
        std::stringstream ss;
        ss << '"';
        for (char c : str) {
            if (c == '"') ss << '"';
            ss << c;
        }
        ss << '"';
        return ss.str();
    }

    static std::string escapeXml(const std::string& str) {
        std::string result;
        result.reserve(str.size());
        for (char c : str) {
            switch (c) {
                case '<': result += "&lt;"; break;
                case '>': result += "&gt;"; break;
                case '&': result += "&amp;"; break;
                case '"': result += "&quot;"; break;
                case '\'': result += "&apos;"; break;
                default: result += c;
            }
        }
        return result;
    }

    static std::string normalizeString(const std::string& str, bool caseSensitive = false) {
        if (caseSensitive) return str;
        std::string normalized = str;
        std::transform(normalized.begin(), normalized.end(),
                      normalized.begin(), ::tolower);
        return normalized;
    }

    static size_t getCurrentMemoryUsage() {
        #ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX pmc;
        GetProcessMemoryInfo(GetCurrentProcess(), 
                           (PROCESS_MEMORY_COUNTERS*)&pmc, 
                           sizeof(pmc));
        return pmc.WorkingSetSize;
        #else
        std::ifstream statm("/proc/self/statm");
        size_t size, resident, share, text, lib, data, dt;
        statm >> size >> resident >> share >> text >> lib >> data >> dt;
        return resident * sysconf(_SC_PAGE_SIZE);
        #endif
    }
};

} // namespace test
} // namespace meshtastic

// Helper macros for common functionality
#define FORMAT_TIMESTAMP(tp) \
    meshtastic::test::TestCommon::formatTimestamp(tp)

#define FORMAT_SHORT_TIMESTAMP(tp) \
    meshtastic::test::TestCommon::formatShortTimestamp(tp)

#define FORMAT_DURATION(duration) \
    meshtastic::test::TestCommon::formatDuration(duration)

#define FORMAT_BYTES(bytes) \
    meshtastic::test::TestCommon::formatBytes(bytes)

#define NORMALIZE_STRING(str, caseSensitive) \
    meshtastic::test::TestCommon::normalizeString(str, caseSensitive)

#define ESCAPE_CSV(str) \
    meshtastic::test::TestCommon::escapeCsv(str)

#define ESCAPE_XML(str) \
    meshtastic::test::TestCommon::escapeXml(str)

#define GET_CURRENT_MEMORY() \
    meshtastic::test::TestCommon::getCurrentMemoryUsage()