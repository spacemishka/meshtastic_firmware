#pragma once

#include <string>
#include <map>
#include <functional>
#include <memory>
#include "test_config.h"

namespace meshtastic {
namespace test {

/**
 * Configuration migration and versioning support
 */
class ConfigMigration {
public:
    struct Version {
        int major;
        int minor;
        int patch;

        bool operator<(const Version& other) const {
            if (major != other.major) return major < other.major;
            if (minor != other.minor) return minor < other.minor;
            return patch < other.patch;
        }

        std::string toString() const {
            return std::to_string(major) + "." + 
                   std::to_string(minor) + "." + 
                   std::to_string(patch);
        }

        static Version fromString(const std::string& str) {
            Version v{0, 0, 0};
            size_t pos1 = str.find('.');
            size_t pos2 = str.find('.', pos1 + 1);
            if (pos1 != std::string::npos && pos2 != std::string::npos) {
                v.major = std::stoi(str.substr(0, pos1));
                v.minor = std::stoi(str.substr(pos1 + 1, pos2 - pos1 - 1));
                v.patch = std::stoi(str.substr(pos2 + 1));
            }
            return v;
        }
    };

    struct MigrationStep {
        Version fromVersion;
        Version toVersion;
        std::function<void(TestConfig::VisualizationTestConfig&)> migrate;
        std::string description;
    };

    static ConfigMigration& instance() {
        static ConfigMigration migration;
        return migration;
    }

    void registerMigration(const MigrationStep& step) {
        migrations[step.fromVersion] = step;
    }

    bool migrateConfig(TestConfig::VisualizationTestConfig& config,
                      const Version& currentVersion,
                      const Version& targetVersion) {
        if (currentVersion < targetVersion) {
            try {
                auto path = findMigrationPath(currentVersion, targetVersion);
                if (path.empty()) {
                    return false;
                }

                for (const auto& step : path) {
                    step.migrate(config);
                    logMigration(step);
                }
                return true;
            } catch (const std::exception& e) {
                logMigrationError(currentVersion, targetVersion, e.what());
                return false;
            }
        }
        return true;
    }

    std::vector<Version> getAvailableVersions() const {
        std::vector<Version> versions;
        for (const auto& [version, _] : migrations) {
            versions.push_back(version);
        }
        std::sort(versions.begin(), versions.end());
        return versions;
    }

    std::string getMigrationHistory() const {
        return migrationHistory.str();
    }

private:
    std::map<Version, MigrationStep> migrations;
    std::stringstream migrationHistory;

    std::vector<MigrationStep> findMigrationPath(const Version& from, 
                                                const Version& to) {
        std::vector<MigrationStep> path;
        Version current = from;

        while (current < to) {
            auto it = findNextStep(current, to);
            if (it == migrations.end()) {
                throw std::runtime_error(
                    "No migration path found from " + current.toString() +
                    " to " + to.toString()
                );
            }

            path.push_back(it->second);
            current = it->second.toVersion;
        }

        return path;
    }

    std::map<Version, MigrationStep>::const_iterator 
    findNextStep(const Version& current, const Version& target) {
        auto bestStep = migrations.end();
        Version bestNext{99, 99, 99};

        for (auto it = migrations.begin(); it != migrations.end(); ++it) {
            if (it->first == current && it->second.toVersion <= target) {
                if (it->second.toVersion < bestNext) {
                    bestStep = it;
                    bestNext = it->second.toVersion;
                }
            }
        }

        return bestStep;
    }

    void logMigration(const MigrationStep& step) {
        migrationHistory << "Migrating from " << step.fromVersion.toString()
                        << " to " << step.toVersion.toString()
                        << ": " << step.description << "\n";
    }

    void logMigrationError(const Version& from, const Version& to, 
                          const std::string& error) {
        migrationHistory << "ERROR migrating from " << from.toString()
                        << " to " << to.toString()
                        << ": " << error << "\n";
    }

public:
    // Common migration steps
    static void registerCommonMigrations() {
        auto& instance = ConfigMigration::instance();

        // Version 1.0.0 to 1.1.0: Add visualization settings
        instance.registerMigration({
            {1, 0, 0}, {1, 1, 0},
            [](TestConfig::VisualizationTestConfig& config) {
                config.visualization.enableAnimations = true;
                config.visualization.enableInteractive = true;
                config.visualization.theme = "default";
            },
            "Added visualization settings"
        });

        // Version 1.1.0 to 1.2.0: Add performance metrics
        instance.registerMigration({
            {1, 1, 0}, {1, 2, 0},
            [](TestConfig::VisualizationTestConfig& config) {
                config.performance.measureMemory = true;
                config.performance.minDataSize = 64;
                config.performance.maxDataSize = 16384;
            },
            "Added performance metrics"
        });

        // Version 1.2.0 to 1.3.0: Add validation settings
        instance.registerMigration({
            {1, 2, 0}, {1, 3, 0},
            [](TestConfig::VisualizationTestConfig& config) {
                config.validation.validateSVG = true;
                config.validation.checkMemoryLeaks = true;
                config.validation.verifyOutput = true;
            },
            "Added validation settings"
        });

        // Version 1.3.0 to 2.0.0: Major update with stress testing
        instance.registerMigration({
            {1, 3, 0}, {2, 0, 0},
            [](TestConfig::VisualizationTestConfig& config) {
                config.stress.concurrentThreads = std::thread::hardware_concurrency();
                config.stress.durationMinutes = 30;
                config.stress.peakMemoryLimitMB = 1024;
                config.stress.abortOnError = true;
            },
            "Added stress testing capabilities"
        });
    }
};

} // namespace test
} // namespace meshtastic

// Helper macros for configuration migration
#define MIGRATE_CONFIG(config, from, to) \
    meshtastic::test::ConfigMigration::instance().migrateConfig(config, from, to)

#define GET_MIGRATION_HISTORY() \
    meshtastic::test::ConfigMigration::instance().getMigrationHistory()

#define GET_AVAILABLE_VERSIONS() \
    meshtastic::test::ConfigMigration::instance().getAvailableVersions()