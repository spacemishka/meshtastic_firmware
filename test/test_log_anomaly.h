#pragma once

#include <string>
#include <vector>
#include <deque>
#include <algorithm>
#include <cmath>
#include <numeric>
#include "test_log_analyzer.h"

namespace meshtastic {
namespace test {

/**
 * Log anomaly detection system
 */
class LogAnomalyDetector {
public:
    struct WindowStats {
        double mean;
        double stddev;
        double median;
        size_t count;
        std::chrono::milliseconds duration;
    };

    struct AnomalyConfig {
        size_t windowSize = 100;          // Number of entries per window
        double zScoreThreshold = 3.0;     // Standard deviations for z-score
        double rateChangeThreshold = 2.0;  // Factor for rate changes
        size_t minSamples = 30;           // Minimum samples for detection
        std::chrono::minutes maxGap{5};   // Maximum gap between entries
        bool detectBursts = true;         // Detect message bursts
        bool detectGaps = true;           // Detect unusual gaps
        bool detectPatterns = true;       // Detect unusual patterns
    };

    struct Anomaly {
        enum class Type {
            RATE_SPIKE,
            RATE_DROP,
            PATTERN_BREAK,
            MESSAGE_BURST,
            UNUSUAL_GAP,
            LEVEL_SHIFT,
            CORRELATION_BREAK
        };

        Type type;
        std::chrono::system_clock::time_point timestamp;
        std::string description;
        double severity;
        std::vector<LogAnalyzer::LogEntry> relatedEntries;
    };

    struct AnalysisWindow {
        std::deque<LogAnalyzer::LogEntry> entries;
        WindowStats stats;
        std::chrono::system_clock::time_point startTime;
        std::chrono::system_clock::time_point endTime;
    };

    static LogAnomalyDetector& instance() {
        static LogAnomalyDetector detector;
        return detector;
    }

    std::vector<Anomaly> detectAnomalies(
        const std::vector<LogAnalyzer::LogEntry>& logs,
        const AnomalyConfig& config = AnomalyConfig()) {
        
        std::vector<Anomaly> anomalies;
        if (logs.size() < config.minSamples) return anomalies;

        // Initialize sliding window
        AnalysisWindow window;
        initializeWindow(window, logs, config);

        // Analyze each window
        for (size_t i = config.windowSize; i < logs.size(); i++) {
            // Update window
            window.entries.pop_front();
            window.entries.push_back(logs[i]);
            updateWindowStats(window);

            // Detect anomalies
            detectRateAnomalies(window, logs[i], config, anomalies);
            if (config.detectBursts) {
                detectMessageBursts(window, logs[i], config, anomalies);
            }
            if (config.detectGaps) {
                detectUnusualGaps(window, logs[i], config, anomalies);
            }
            if (config.detectPatterns) {
                detectPatternBreaks(window, logs[i], config, anomalies);
            }
        }

        // Post-process anomalies
        mergeRelatedAnomalies(anomalies);
        rankAnomalies(anomalies);

        return anomalies;
    }

    std::string generateAnomalyReport(const std::vector<Anomaly>& anomalies) {
        std::stringstream report;
        report << "Log Anomaly Detection Report\n"
               << "===========================\n\n";

        if (anomalies.empty()) {
            report << "No anomalies detected.\n";
            return report.str();
        }

        // Group anomalies by type
        std::map<Anomaly::Type, std::vector<Anomaly>> grouped;
        for (const auto& anomaly : anomalies) {
            grouped[anomaly.type].push_back(anomaly);
        }

        // Report for each type
        for (const auto& [type, typeAnomalies] : grouped) {
            report << getAnomalyTypeName(type) << "\n"
                   << std::string(getAnomalyTypeName(type).length(), '-') << "\n"
                   << "Count: " << typeAnomalies.size() << "\n\n";

            // Sort by severity
            auto sortedAnomalies = typeAnomalies;
            std::sort(sortedAnomalies.begin(), sortedAnomalies.end(),
                [](const Anomaly& a, const Anomaly& b) {
                    return a.severity > b.severity;
                });

            // Report details
            for (const auto& anomaly : sortedAnomalies) {
                report << "Time: " << formatTimestamp(anomaly.timestamp) << "\n"
                       << "Severity: " << std::fixed << std::setprecision(2) 
                       << anomaly.severity << "\n"
                       << "Description: " << anomaly.description << "\n";

                if (!anomaly.relatedEntries.empty()) {
                    report << "Related Entries:\n";
                    for (const auto& entry : anomaly.relatedEntries) {
                        report << "  " << formatLogEntry(entry) << "\n";
                    }
                }
                report << "\n";
            }
            report << "\n";
        }

        return report.str();
    }

private:
    void initializeWindow(AnalysisWindow& window,
                         const std::vector<LogAnalyzer::LogEntry>& logs,
                         const AnomalyConfig& config) {
        window.entries.clear();
        for (size_t i = 0; i < config.windowSize; i++) {
            window.entries.push_back(logs[i]);
        }
        window.startTime = window.entries.front().timestamp;
        window.endTime = window.entries.back().timestamp;
        updateWindowStats(window);
    }

    void updateWindowStats(AnalysisWindow& window) {
        std::vector<double> rates;
        std::vector<std::chrono::milliseconds> intervals;

        // Calculate intervals
        for (size_t i = 1; i < window.entries.size(); i++) {
            auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(
                window.entries[i].timestamp - window.entries[i-1].timestamp);
            intervals.push_back(interval);
            rates.push_back(1000.0 / interval.count());
        }

        // Calculate statistics
        window.stats.mean = calculateMean(rates);
        window.stats.stddev = calculateStdDev(rates, window.stats.mean);
        window.stats.median = calculateMedian(rates);
        window.stats.count = window.entries.size();
        window.stats.duration = std::accumulate(intervals.begin(), intervals.end(),
            std::chrono::milliseconds(0));
    }

    void detectRateAnomalies(const AnalysisWindow& window,
                            const LogAnalyzer::LogEntry& currentEntry,
                            const AnomalyConfig& config,
                            std::vector<Anomaly>& anomalies) {
        auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentEntry.timestamp - window.entries.back().timestamp);
        double rate = 1000.0 / interval.count();
        double zScore = (rate - window.stats.mean) / window.stats.stddev;

        if (std::abs(zScore) > config.zScoreThreshold) {
            Anomaly anomaly;
            anomaly.timestamp = currentEntry.timestamp;
            anomaly.severity = std::abs(zScore);

            if (rate > window.stats.mean) {
                anomaly.type = Anomaly::Type::RATE_SPIKE;
                anomaly.description = "Message rate spike detected";
            } else {
                anomaly.type = Anomaly::Type::RATE_DROP;
                anomaly.description = "Message rate drop detected";
            }

            anomaly.relatedEntries = {window.entries.back(), currentEntry};
            anomalies.push_back(anomaly);
        }
    }

    void detectMessageBursts(const AnalysisWindow& window,
                           const LogAnalyzer::LogEntry& currentEntry,
                           const AnomalyConfig& config,
                           std::vector<Anomaly>& anomalies) {
        // Count messages in short time window
        auto cutoff = currentEntry.timestamp - std::chrono::seconds(1);
        size_t burstCount = 0;
        
        for (auto it = window.entries.rbegin(); 
             it != window.entries.rend() && it->timestamp > cutoff; ++it) {
            burstCount++;
        }

        if (burstCount > window.stats.mean * config.rateChangeThreshold) {
            Anomaly anomaly;
            anomaly.type = Anomaly::Type::MESSAGE_BURST;
            anomaly.timestamp = currentEntry.timestamp;
            anomaly.severity = static_cast<double>(burstCount) / window.stats.mean;
            anomaly.description = "Message burst detected: " + 
                                std::to_string(burstCount) + " messages in 1s";
            
            // Collect burst messages
            std::vector<LogAnalyzer::LogEntry> burstEntries;
            auto it = window.entries.rbegin();
            while (burstEntries.size() < burstCount && it != window.entries.rend()) {
                burstEntries.push_back(*it++);
            }
            anomaly.relatedEntries = burstEntries;
            
            anomalies.push_back(anomaly);
        }
    }

    void detectUnusualGaps(const AnalysisWindow& window,
                          const LogAnalyzer::LogEntry& currentEntry,
                          const AnomalyConfig& config,
                          std::vector<Anomaly>& anomalies) {
        auto gap = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentEntry.timestamp - window.entries.back().timestamp);

        if (gap > config.maxGap) {
            Anomaly anomaly;
            anomaly.type = Anomaly::Type::UNUSUAL_GAP;
            anomaly.timestamp = currentEntry.timestamp;
            anomaly.severity = static_cast<double>(gap.count()) / 
                             config.maxGap.count();
            anomaly.description = "Unusual gap detected: " + 
                                std::to_string(gap.count()) + "ms";
            anomaly.relatedEntries = {window.entries.back(), currentEntry};
            
            anomalies.push_back(anomaly);
        }
    }

    void detectPatternBreaks(const AnalysisWindow& window,
                            const LogAnalyzer::LogEntry& currentEntry,
                            const AnomalyConfig& config,
                            std::vector<Anomaly>& anomalies) {
        // Analyze level patterns
        std::map<TestLogger::Level, size_t> levelCounts;
        for (const auto& entry : window.entries) {
            levelCounts[entry.level]++;
        }

        double expectedProb = static_cast<double>(
            levelCounts[currentEntry.level]) / window.entries.size();
        
        if (expectedProb < 0.1) {  // Rare level occurrence
            Anomaly anomaly;
            anomaly.type = Anomaly::Type::LEVEL_SHIFT;
            anomaly.timestamp = currentEntry.timestamp;
            anomaly.severity = 1.0 - expectedProb;
            anomaly.description = "Unusual log level pattern detected";
            anomaly.relatedEntries = {currentEntry};
            
            anomalies.push_back(anomaly);
        }
    }

    void mergeRelatedAnomalies(std::vector<Anomaly>& anomalies) {
        if (anomalies.size() < 2) return;

        std::vector<Anomaly> merged;
        std::sort(anomalies.begin(), anomalies.end(),
            [](const Anomaly& a, const Anomaly& b) {
                return a.timestamp < b.timestamp;
            });

        for (size_t i = 0; i < anomalies.size(); i++) {
            if (i == 0 || 
                std::chrono::duration_cast<std::chrono::seconds>(
                    anomalies[i].timestamp - anomalies[i-1].timestamp
                ).count() > 5) {
                merged.push_back(anomalies[i]);
            } else {
                // Merge with previous anomaly
                auto& prev = merged.back();
                prev.severity = std::max(prev.severity, anomalies[i].severity);
                prev.relatedEntries.insert(
                    prev.relatedEntries.end(),
                    anomalies[i].relatedEntries.begin(),
                    anomalies[i].relatedEntries.end()
                );
            }
        }

        anomalies = merged;
    }

    void rankAnomalies(std::vector<Anomaly>& anomalies) {
        // Normalize severity scores
        if (anomalies.empty()) return;

        double maxSeverity = std::max_element(
            anomalies.begin(), anomalies.end(),
            [](const Anomaly& a, const Anomaly& b) {
                return a.severity < b.severity;
            }
        )->severity;

        for (auto& anomaly : anomalies) {
            anomaly.severity /= maxSeverity;
        }
    }

    std::string getAnomalyTypeName(Anomaly::Type type) const {
        switch (type) {
            case Anomaly::Type::RATE_SPIKE:
                return "Rate Spike";
            case Anomaly::Type::RATE_DROP:
                return "Rate Drop";
            case Anomaly::Type::PATTERN_BREAK:
                return "Pattern Break";
            case Anomaly::Type::MESSAGE_BURST:
                return "Message Burst";
            case Anomaly::Type::UNUSUAL_GAP:
                return "Unusual Gap";
            case Anomaly::Type::LEVEL_SHIFT:
                return "Level Shift";
            case Anomaly::Type::CORRELATION_BREAK:
                return "Correlation Break";
            default:
                return "Unknown";
        }
    }

    std::string formatTimestamp(const std::chrono::system_clock::time_point& tp) const {
        auto time = std::chrono::system_clock::to_time_t(tp);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    std::string formatLogEntry(const LogAnalyzer::LogEntry& entry) const {
        std::stringstream ss;
        ss << formatTimestamp(entry.timestamp) << " ["
           << TestLogger::instance().levelToString(entry.level) << "] "
           << entry.message;
        return ss.str();
    }

    double calculateMean(const std::vector<double>& values) const {
        if (values.empty()) return 0.0;
        return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    }

    double calculateStdDev(const std::vector<double>& values, double mean) const {
        if (values.size() < 2) return 0.0;
        double sumSq = std::accumulate(values.begin(), values.end(), 0.0,
            [mean](double acc, double val) {
                double diff = val - mean;
                return acc + diff * diff;
            });
        return std::sqrt(sumSq / (values.size() - 1));
    }

    double calculateMedian(std::vector<double> values) const {
        if (values.empty()) return 0.0;
        std::sort(values.begin(), values.end());
        if (values.size() % 2 == 0) {
            return (values[values.size()/2 - 1] + values[values.size()/2]) / 2.0;
        }
        return values[values.size()/2];
    }
};

} // namespace test
} // namespace meshtastic

// Helper macros for anomaly detection
#define DETECT_LOG_ANOMALIES(logs) \
    meshtastic::test::LogAnomalyDetector::instance().detectAnomalies(logs)

#define GENERATE_ANOMALY_REPORT(anomalies) \
    meshtastic::test::LogAnomalyDetector::instance().generateAnomalyReport(anomalies)