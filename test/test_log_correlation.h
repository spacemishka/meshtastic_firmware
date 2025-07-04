#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include "test_log_analyzer.h"
#include "test_log_anomaly.h"

namespace meshtastic {
namespace test {

/**
 * Log correlation and pattern analysis
 */
class LogCorrelation {
public:
    struct CorrelationPattern {
        std::vector<LogAnalyzer::LogEntry> sequence;
        size_t occurrences;
        std::chrono::milliseconds averageInterval;
        double confidence;
        std::string description;
    };

    struct CorrelationConfig {
        size_t minSequenceLength = 2;      // Minimum events in sequence
        size_t maxSequenceLength = 5;      // Maximum events in sequence
        size_t minOccurrences = 3;         // Minimum pattern occurrences
        double minConfidence = 0.7;        // Minimum pattern confidence
        std::chrono::seconds maxInterval{5}; // Maximum interval between related events
        bool ignoreTimestamps = false;     // Ignore timing in patterns
        bool caseSensitive = false;        // Case-sensitive matching
    };

    struct CorrelationResult {
        std::vector<CorrelationPattern> patterns;
        std::map<std::string, std::set<std::string>> dependencies;
        std::vector<std::pair<std::string, std::string>> causality;
        double correlationScore;
    };

    static LogCorrelation& instance() {
        static LogCorrelation correlation;
        return correlation;
    }

    CorrelationResult analyze(
        const std::vector<LogAnalyzer::LogEntry>& logs,
        const CorrelationConfig& config = CorrelationConfig()) {
        
        CorrelationResult result;
        
        // Find repeating sequences
        findPatterns(logs, config, result);
        
        // Analyze dependencies
        analyzeDependencies(logs, result);
        
        // Detect causality
        detectCausality(logs, result);
        
        // Calculate overall correlation score
        result.correlationScore = calculateCorrelationScore(result);
        
        return result;
    }

    std::string generateReport(const CorrelationResult& result) {
        std::stringstream report;
        report << "Log Correlation Analysis Report\n"
               << "==============================\n\n"
               << "Overall Correlation Score: " << std::fixed << std::setprecision(2)
               << result.correlationScore << "\n\n";

        // Report patterns
        report << "Repeating Patterns\n"
               << "-----------------\n";
        if (result.patterns.empty()) {
            report << "No significant patterns detected.\n\n";
        } else {
            for (const auto& pattern : result.patterns) {
                report << "Pattern (Confidence: " << pattern.confidence << ")\n"
                       << "Occurrences: " << pattern.occurrences << "\n"
                       << "Average Interval: " << pattern.averageInterval.count() << "ms\n"
                       << "Sequence:\n";
                
                for (const auto& entry : pattern.sequence) {
                    report << "  " << formatLogEntry(entry) << "\n";
                }
                report << "\n";
            }
        }

        // Report dependencies
        report << "Dependencies\n"
               << "------------\n";
        if (result.dependencies.empty()) {
            report << "No dependencies detected.\n\n";
        } else {
            for (const auto& [source, targets] : result.dependencies) {
                report << source << " triggers:\n";
                for (const auto& target : targets) {
                    report << "  - " << target << "\n";
                }
                report << "\n";
            }
        }

        // Report causality
        report << "Causality Chains\n"
               << "----------------\n";
        if (result.causality.empty()) {
            report << "No causality chains detected.\n\n";
        } else {
            for (const auto& [cause, effect] : result.causality) {
                report << cause << " -> " << effect << "\n";
            }
        }

        return report.str();
    }

private:
    void findPatterns(const std::vector<LogAnalyzer::LogEntry>& logs,
                     const CorrelationConfig& config,
                     CorrelationResult& result) {
        // Store potential patterns and their occurrences
        std::map<std::vector<std::string>, std::vector<size_t>> sequenceIndices;
        
        // Scan for sequences
        for (size_t length = config.minSequenceLength; 
             length <= config.maxSequenceLength; length++) {
            
            for (size_t i = 0; i <= logs.size() - length; i++) {
                std::vector<std::string> sequence;
                bool validSequence = true;
                
                // Build sequence
                for (size_t j = 0; j < length; j++) {
                    if (!config.ignoreTimestamps &&
                        j > 0 &&
                        logs[i + j].timestamp - logs[i + j - 1].timestamp > 
                        config.maxInterval) {
                        validSequence = false;
                        break;
                    }
                    sequence.push_back(normalizeMessage(
                        logs[i + j].message, config.caseSensitive));
                }
                
                if (validSequence) {
                    sequenceIndices[sequence].push_back(i);
                }
            }
        }

        // Filter and convert to patterns
        for (const auto& [sequence, indices] : sequenceIndices) {
            if (indices.size() >= config.minOccurrences) {
                CorrelationPattern pattern;
                
                // Store sequence entries
                for (size_t i = 0; i < sequence.size(); i++) {
                    pattern.sequence.push_back(logs[indices[0] + i]);
                }
                
                pattern.occurrences = indices.size();
                
                // Calculate average interval
                std::chrono::milliseconds totalInterval(0);
                size_t intervalCount = 0;
                
                for (size_t i = 1; i < indices.size(); i++) {
                    auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(
                        logs[indices[i]].timestamp - logs[indices[i-1]].timestamp);
                    totalInterval += interval;
                    intervalCount++;
                }
                
                pattern.averageInterval = totalInterval / intervalCount;
                
                // Calculate confidence
                pattern.confidence = calculatePatternConfidence(sequence, logs);
                
                if (pattern.confidence >= config.minConfidence) {
                    pattern.description = generatePatternDescription(pattern);
                    result.patterns.push_back(pattern);
                }
            }
        }
    }

    void analyzeDependencies(const std::vector<LogAnalyzer::LogEntry>& logs,
                           CorrelationResult& result) {
        const auto timeWindow = std::chrono::seconds(5);
        
        for (size_t i = 0; i < logs.size(); i++) {
            const auto& currentEntry = logs[i];
            std::set<std::string> effects;
            
            // Look for consistent effects within time window
            for (size_t j = i + 1; j < logs.size() &&
                 logs[j].timestamp - currentEntry.timestamp <= timeWindow; j++) {
                effects.insert(normalizeMessage(logs[j].message, false));
            }
            
            // Check if effects are consistent across other occurrences
            for (const auto& effect : effects) {
                bool consistent = true;
                size_t occurrences = 0;
                
                for (size_t j = 0; j < logs.size(); j++) {
                    if (normalizeMessage(logs[j].message, false) == 
                        normalizeMessage(currentEntry.message, false)) {
                        occurrences++;
                        bool foundEffect = false;
                        
                        // Look for effect within window
                        for (size_t k = j + 1; k < logs.size() &&
                             logs[k].timestamp - logs[j].timestamp <= timeWindow; k++) {
                            if (normalizeMessage(logs[k].message, false) == effect) {
                                foundEffect = true;
                                break;
                            }
                        }
                        
                        if (!foundEffect) {
                            consistent = false;
                            break;
                        }
                    }
                }
                
                if (consistent && occurrences >= 3) {
                    result.dependencies[currentEntry.message].insert(effect);
                }
            }
        }
    }

    void detectCausality(const std::vector<LogAnalyzer::LogEntry>& logs,
                        CorrelationResult& result) {
        const auto maxDelay = std::chrono::seconds(1);
        std::map<std::pair<std::string, std::string>, size_t> transitions;
        
        // Count message transitions within time window
        for (size_t i = 0; i < logs.size() - 1; i++) {
            for (size_t j = i + 1; j < logs.size() &&
                 logs[j].timestamp - logs[i].timestamp <= maxDelay; j++) {
                transitions[{normalizeMessage(logs[i].message, false),
                           normalizeMessage(logs[j].message, false)}]++;
            }
        }
        
        // Filter significant transitions
        for (const auto& [transition, count] : transitions) {
            if (count >= 3) {
                result.causality.push_back(transition);
            }
        }
    }

    double calculateCorrelationScore(const CorrelationResult& result) {
        if (result.patterns.empty()) return 0.0;
        
        double patternScore = std::accumulate(
            result.patterns.begin(),
            result.patterns.end(),
            0.0,
            [](double acc, const CorrelationPattern& pattern) {
                return acc + pattern.confidence;
            }) / result.patterns.size();
            
        double dependencyScore = result.dependencies.empty() ? 0.0 : 1.0;
        double causalityScore = result.causality.empty() ? 0.0 : 1.0;
        
        return (patternScore + dependencyScore + causalityScore) / 3.0;
    }

    double calculatePatternConfidence(const std::vector<std::string>& sequence,
                                    const std::vector<LogAnalyzer::LogEntry>& logs) {
        size_t matches = 0;
        size_t totalPossible = logs.size() - sequence.size() + 1;
        
        for (size_t i = 0; i <= logs.size() - sequence.size(); i++) {
            bool match = true;
            for (size_t j = 0; j < sequence.size(); j++) {
                if (normalizeMessage(logs[i + j].message, false) != sequence[j]) {
                    match = false;
                    break;
                }
            }
            if (match) matches++;
        }
        
        return static_cast<double>(matches) / totalPossible;
    }

    std::string normalizeMessage(const std::string& message, bool caseSensitive) {
        if (caseSensitive) return message;
        
        std::string normalized = message;
        std::transform(normalized.begin(), normalized.end(),
                      normalized.begin(), ::tolower);
                      
        // Remove variable parts (timestamps, IDs, etc.)
        static const std::vector<std::regex> patterns = {
            std::regex("\\d{4}-\\d{2}-\\d{2}"),  // dates
            std::regex("\\d{2}:\\d{2}:\\d{2}"),  // times
            std::regex("0x[0-9a-f]+"),           // hex numbers
            std::regex("\\d+(?:\\.\\d+)?"),      // numbers
            std::regex("\"[^\"]*\""),            // quoted strings
            std::regex("\\[.*?\\]")              // bracketed content
        };
        
        for (const auto& pattern : patterns) {
            normalized = std::regex_replace(normalized, pattern, "***");
        }
        
        return normalized;
    }

    std::string generatePatternDescription(const CorrelationPattern& pattern) {
        std::stringstream ss;
        ss << "Sequence of " << pattern.sequence.size() << " events occurring "
           << pattern.occurrences << " times with average interval of "
           << pattern.averageInterval.count() << "ms";
        return ss.str();
    }

    std::string formatLogEntry(const LogAnalyzer::LogEntry& entry) {
        std::stringstream ss;
        auto time = std::chrono::system_clock::to_time_t(entry.timestamp);
        ss << std::put_time(std::localtime(&time), "%H:%M:%S") << " ["
           << TestLogger::instance().levelToString(entry.level) << "] "
           << entry.message;
        return ss.str();
    }
};

} // namespace test
} // namespace meshtastic

// Helper macros for correlation analysis
#define ANALYZE_LOG_CORRELATION(logs) \
    meshtastic::test::LogCorrelation::instance().analyze(logs)

#define GENERATE_CORRELATION_REPORT(result) \
    meshtastic::test::LogCorrelation::instance().generateReport(result)