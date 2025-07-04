#pragma once

#include <string>
#include <vector>
#include <map>
#include <regex>
#include <algorithm>
#include <chrono>
#include "test_logger.h"

namespace meshtastic {
namespace test {

/**
 * Log analysis and pattern detection
 */
class LogAnalyzer {
public:
    struct LogEntry {
        std::chrono::system_clock::time_point timestamp;
        TestLogger::Level level;
        std::string source;
        int line;
        std::string message;
    };

    struct Pattern {
        std::string name;
        std::regex pattern;
        TestLogger::Level minLevel;
        bool isError;
        std::string description;
    };

    struct AnalysisResult {
        struct Issue {
            std::string pattern;
            std::vector<LogEntry> occurrences;
            std::string description;
            bool isError;
        };

        struct Statistic {
            size_t totalEntries;
            std::map<TestLogger::Level, size_t> levelCounts;
            std::map<std::string, size_t> sourceCounts;
            double averageRate;  // entries per second
            std::chrono::milliseconds peakInterval;
        };

        std::vector<Issue> issues;
        Statistic stats;
        std::chrono::system_clock::time_point analysisTime;
    };

    static LogAnalyzer& instance() {
        static LogAnalyzer analyzer;
        return analyzer;
    }

    void addPattern(const std::string& name,
                   const std::string& pattern,
                   TestLogger::Level minLevel,
                   bool isError,
                   const std::string& description) {
        patterns_.push_back({
            name,
            std::regex(pattern),
            minLevel,
            isError,
            description
        });
    }

    void addDefaultPatterns() {
        // Error patterns
        addPattern(
            "Exception",
            "exception|error|failure|failed|crash",
            TestLogger::Level::ERROR,
            true,
            "Detected error or exception condition"
        );

        addPattern(
            "Timeout",
            "timeout|timed out|deadline exceeded",
            TestLogger::Level::WARNING,
            true,
            "Operation timeout detected"
        );

        addPattern(
            "Resource Exhaustion",
            "out of memory|resource exhausted|capacity exceeded",
            TestLogger::Level::ERROR,
            true,
            "Resource exhaustion detected"
        );

        // Warning patterns
        addPattern(
            "Performance",
            "slow|delayed|lag|performance|latency",
            TestLogger::Level::WARNING,
            false,
            "Performance issue detected"
        );

        addPattern(
            "Retry",
            "retry|retrying|attempt",
            TestLogger::Level::WARNING,
            false,
            "Operation retry detected"
        );

        // Info patterns
        addPattern(
            "Configuration",
            "config|configuration|setting|parameter",
            TestLogger::Level::INFO,
            false,
            "Configuration change detected"
        );

        addPattern(
            "State Change",
            "started|stopped|initialized|completed|begin|end",
            TestLogger::Level::INFO,
            false,
            "State transition detected"
        );
    }

    AnalysisResult analyze(const std::vector<std::string>& logs) {
        AnalysisResult result;
        result.analysisTime = std::chrono::system_clock::now();

        std::vector<LogEntry> entries = parseLogs(logs);
        result.stats = computeStatistics(entries);
        result.issues = detectIssues(entries);

        return result;
    }

    std::string generateReport(const AnalysisResult& result) {
        std::stringstream report;
        
        // Header
        report << "Log Analysis Report\n"
               << "==================\n\n"
               << "Analysis Time: " << formatTimestamp(result.analysisTime) << "\n\n";

        // Statistics
        report << "Statistics\n"
               << "----------\n"
               << "Total Entries: " << result.stats.totalEntries << "\n"
               << "Average Rate: " << std::fixed << std::setprecision(2) 
               << result.stats.averageRate << " entries/sec\n"
               << "Peak Interval: " << result.stats.peakInterval.count() << "ms\n\n";

        // Level distribution
        report << "Log Level Distribution:\n";
        for (const auto& [level, count] : result.stats.levelCounts) {
            report << "  " << std::left << std::setw(10) 
                   << TestLogger::instance().levelToString(level) 
                   << ": " << count << "\n";
        }
        report << "\n";

        // Source distribution
        report << "Top Sources:\n";
        std::vector<std::pair<std::string, size_t>> sources(
            result.stats.sourceCounts.begin(),
            result.stats.sourceCounts.end()
        );
        std::sort(sources.begin(), sources.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (size_t i = 0; i < std::min(sources.size(), size_t(5)); i++) {
            report << "  " << std::left << std::setw(30) << sources[i].first 
                   << ": " << sources[i].second << "\n";
        }
        report << "\n";

        // Issues
        report << "Issues\n"
               << "------\n";
        if (result.issues.empty()) {
            report << "No issues detected\n\n";
        } else {
            for (const auto& issue : result.issues) {
                report << "[" << (issue.isError ? "ERROR" : "WARNING") << "] "
                       << issue.pattern << "\n"
                       << "Description: " << issue.description << "\n"
                       << "Occurrences: " << issue.occurrences.size() << "\n";
                
                for (const auto& entry : issue.occurrences) {
                    report << "  " << formatTimestamp(entry.timestamp) 
                           << " " << entry.source << ":" << entry.line 
                           << ": " << entry.message << "\n";
                }
                report << "\n";
            }
        }

        return report.str();
    }

private:
    std::vector<Pattern> patterns_;

    std::vector<LogEntry> parseLogs(const std::vector<std::string>& logs) {
        std::vector<LogEntry> entries;
        
        // Regular expression for log entry parsing
        std::regex logPattern(
            R"(\[(.*?)\]\s*\[(.*?)\]\s*(?:\[(.*?):(.*?)\])?\s*(.*))"
        );

        for (const auto& log : logs) {
            std::smatch matches;
            if (std::regex_match(log, matches, logPattern)) {
                LogEntry entry;
                entry.timestamp = parseTimestamp(matches[1]);
                entry.level = parseLevel(matches[2]);
                entry.source = matches[3];
                entry.line = matches[4].length() > 0 ? std::stoi(matches[4]) : 0;
                entry.message = matches[5];
                entries.push_back(entry);
            }
        }

        return entries;
    }

    AnalysisResult::Statistic computeStatistics(const std::vector<LogEntry>& entries) {
        AnalysisResult::Statistic stats;
        stats.totalEntries = entries.size();

        if (entries.empty()) {
            stats.averageRate = 0;
            stats.peakInterval = std::chrono::milliseconds(0);
            return stats;
        }

        // Compute level counts
        for (const auto& entry : entries) {
            stats.levelCounts[entry.level]++;
            stats.sourceCounts[entry.source]++;
        }

        // Compute rate and intervals
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            entries.back().timestamp - entries.front().timestamp);
        stats.averageRate = duration.count() > 0 ?
            static_cast<double>(entries.size()) / duration.count() : 0;

        std::chrono::milliseconds maxInterval(0);
        for (size_t i = 1; i < entries.size(); i++) {
            auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(
                entries[i].timestamp - entries[i-1].timestamp);
            if (interval > maxInterval) {
                maxInterval = interval;
            }
        }
        stats.peakInterval = maxInterval;

        return stats;
    }

    std::vector<AnalysisResult::Issue> detectIssues(const std::vector<LogEntry>& entries) {
        std::vector<AnalysisResult::Issue> issues;

        for (const auto& pattern : patterns_) {
            std::vector<LogEntry> matches;
            
            for (const auto& entry : entries) {
                if (entry.level >= pattern.minLevel &&
                    std::regex_search(entry.message, pattern.pattern)) {
                    matches.push_back(entry);
                }
            }

            if (!matches.empty()) {
                issues.push_back({
                    pattern.name,
                    matches,
                    pattern.description,
                    pattern.isError
                });
            }
        }

        return issues;
    }

    std::chrono::system_clock::time_point parseTimestamp(const std::string& ts) {
        std::tm tm = {};
        std::istringstream ss(ts);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    TestLogger::Level parseLevel(const std::string& level) {
        if (level == "DEBUG") return TestLogger::Level::DEBUG;
        if (level == "INFO") return TestLogger::Level::INFO;
        if (level == "WARN") return TestLogger::Level::WARNING;
        if (level == "ERROR") return TestLogger::Level::ERROR;
        if (level == "CRITICAL") return TestLogger::Level::CRITICAL;
        return TestLogger::Level::INFO;
    }

    std::string formatTimestamp(const std::chrono::system_clock::time_point& tp) {
        auto time = std::chrono::system_clock::to_time_t(tp);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

} // namespace test
} // namespace meshtastic

// Helper macros for log analysis
#define ANALYZE_LOGS(logs) \
    meshtastic::test::LogAnalyzer::instance().analyze(logs)

#define GENERATE_LOG_REPORT(result) \
    meshtastic::test::LogAnalyzer::instance().generateReport(result)

#define ADD_LOG_PATTERN(name, pattern, level, isError, desc) \
    meshtastic::test::LogAnalyzer::instance().addPattern(name, pattern, level, isError, desc)

#define ADD_DEFAULT_PATTERNS() \
    meshtastic::test::LogAnalyzer::instance().addDefaultPatterns()