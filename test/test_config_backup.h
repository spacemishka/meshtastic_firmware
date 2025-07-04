#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <optional>
#include <zlib.h>
#include "test_config.h"
#include "test_config_migration.h"

namespace meshtastic {
namespace test {

/**
 * Configuration backup and restore system with compression
 */
class ConfigBackup {
public:
    struct BackupInfo {
        std::string filename;
        std::chrono::system_clock::time_point timestamp;
        ConfigMigration::Version version;
        std::string description;
        size_t originalSize;
        size_t compressedSize;
    };

    static ConfigBackup& instance() {
        static ConfigBackup backup;
        return backup;
    }

    bool createBackup(const TestConfig::VisualizationTestConfig& config,
                     const ConfigMigration::Version& version,
                     const std::string& description = "") {
        try {
            std::filesystem::path backupDir = getBackupDirectory();
            std::filesystem::create_directories(backupDir);

            std::string filename = generateBackupFilename();
            std::filesystem::path backupPath = backupDir / filename;

            // Serialize and compress config
            std::string serialized = serializeConfig(config, version, description);
            std::string compressed = compressData(serialized);

            // Save compressed data
            if (saveCompressedData(compressed, backupPath)) {
                removeOldBackups();
                return true;
            }
            return false;

        } catch (const std::exception& e) {
            addError("Backup creation failed: " + std::string(e.what()));
            return false;
        }
    }

    std::unique_ptr<TestConfig::VisualizationTestConfig> 
    restoreBackup(const std::string& filename) {
        try {
            std::filesystem::path backupPath = getBackupDirectory() / filename;
            std::string compressed = loadCompressedData(backupPath);
            std::string serialized = decompressData(compressed);
            return deserializeConfig(serialized);
        } catch (const std::exception& e) {
            addError("Backup restore failed: " + std::string(e.what()));
            return nullptr;
        }
    }

    std::vector<BackupInfo> listBackups() const {
        std::vector<BackupInfo> backups;
        try {
            for (const auto& entry : std::filesystem::directory_iterator(getBackupDirectory())) {
                if (entry.path().extension() == ".backup") {
                    BackupInfo info = readBackupInfo(entry.path());
                    backups.push_back(info);
                }
            }
        } catch (const std::exception& e) {
            addError("Failed to list backups: " + std::string(e.what()));
        }

        std::sort(backups.begin(), backups.end(),
            [](const BackupInfo& a, const BackupInfo& b) {
                return a.timestamp > b.timestamp;
            });

        return backups;
    }

    bool deleteBackup(const std::string& filename) {
        try {
            std::filesystem::path backupPath = getBackupDirectory() / filename;
            return std::filesystem::remove(backupPath);
        } catch (const std::exception& e) {
            addError("Failed to delete backup: " + std::string(e.what()));
            return false;
        }
    }

    std::string getReport() const {
        std::ostringstream report;
        auto backups = listBackups();

        report << "Configuration Backup Report\n"
               << "==========================\n\n"
               << "Total backups: " << backups.size() << "\n\n";

        for (const auto& backup : backups) {
            report << "Filename: " << backup.filename << "\n"
                   << "Timestamp: " << formatTimestamp(backup.timestamp) << "\n"
                   << "Version: " << backup.version.toString() << "\n"
                   << "Original size: " << formatSize(backup.originalSize) << "\n"
                   << "Compressed size: " << formatSize(backup.compressedSize) << "\n"
                   << "Compression ratio: " 
                   << std::fixed << std::setprecision(1)
                   << (100.0 * backup.compressedSize / backup.originalSize) << "%\n";
            if (!backup.description.empty()) {
                report << "Description: " << backup.description << "\n";
            }
            report << "\n";
        }

        if (!errorLog.empty()) {
            report << "Recent Errors:\n";
            for (const auto& error : errorLog) {
                report << "- " << error << "\n";
            }
        }

        return report.str();
    }

private:
    static constexpr size_t MAX_BACKUPS = 10;
    static constexpr size_t MAX_ERROR_LOG = 100;
    mutable std::vector<std::string> errorLog;

    std::filesystem::path getBackupDirectory() const {
        return std::filesystem::path("config_backups");
    }

    std::string generateBackupFilename() const {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << "config_" << std::put_time(std::localtime(&timestamp), "%Y%m%d_%H%M%S")
           << ".backup";
        return ss.str();
    }

    std::string compressData(const std::string& data) {
        std::vector<char> compressed(data.size() * 1.1 + 12); // zlib recommendation
        z_stream stream = {};
        deflateInit(&stream, Z_BEST_COMPRESSION);

        stream.next_in = (Bytef*)data.data();
        stream.avail_in = data.size();
        stream.next_out = (Bytef*)compressed.data();
        stream.avail_out = compressed.size();

        deflate(&stream, Z_FINISH);
        deflateEnd(&stream);

        return std::string(compressed.data(), stream.total_out);
    }

    std::string decompressData(const std::string& compressed) {
        std::vector<char> decompressed;
        z_stream stream = {};
        inflateInit(&stream);

        stream.next_in = (Bytef*)compressed.data();
        stream.avail_in = compressed.size();

        do {
            decompressed.resize(decompressed.size() + 1024);
            stream.next_out = (Bytef*)(decompressed.data() + stream.total_out);
            stream.avail_out = decompressed.size() - stream.total_out;
            inflate(&stream, Z_NO_FLUSH);
        } while (stream.avail_out == 0);

        inflateEnd(&stream);
        decompressed.resize(stream.total_out);

        return std::string(decompressed.data(), decompressed.size());
    }

    std::string serializeConfig(const TestConfig::VisualizationTestConfig& config,
                              const ConfigMigration::Version& version,
                              const std::string& description) {
        std::stringstream ss;
        ss << "VERSION=" << version.toString() << "\n"
           << "TIMESTAMP=" << std::chrono::system_clock::now().time_since_epoch().count() << "\n";
        if (!description.empty()) {
            ss << "DESCRIPTION=" << description << "\n";
        }
        ss << "---\n";
        // Add config serialization here
        return ss.str();
    }

    std::unique_ptr<TestConfig::VisualizationTestConfig>
    deserializeConfig(const std::string& data) {
        auto config = std::make_unique<TestConfig::VisualizationTestConfig>();
        std::istringstream ss(data);
        std::string line;

        while (std::getline(ss, line) && line != "---") {
            // Parse header
        }

        // Add config deserialization here
        return config;
    }

    bool saveCompressedData(const std::string& data, const std::filesystem::path& path) {
        std::ofstream file(path, std::ios::binary);
        return file.write(data.data(), data.size()).good();
    }

    std::string loadCompressedData(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file),
                          std::istreambuf_iterator<char>());
    }

    BackupInfo readBackupInfo(const std::filesystem::path& path) const {
        BackupInfo info;
        info.filename = path.filename().string();
        info.originalSize = std::filesystem::file_size(path);

        std::string compressed = loadCompressedData(path);
        std::string decompressed = decompressData(compressed);
        info.compressedSize = compressed.size();

        std::istringstream ss(decompressed);
        std::string line;
        while (std::getline(ss, line) && line != "---") {
            if (line.starts_with("VERSION=")) {
                info.version = ConfigMigration::Version::fromString(line.substr(8));
            } else if (line.starts_with("TIMESTAMP=")) {
                auto count = std::stoull(line.substr(10));
                info.timestamp = std::chrono::system_clock::time_point(
                    std::chrono::system_clock::duration(count));
            } else if (line.starts_with("DESCRIPTION=")) {
                info.description = line.substr(12);
            }
        }

        return info;
    }

    void removeOldBackups() {
        auto backups = listBackups();
        if (backups.size() > MAX_BACKUPS) {
            for (size_t i = MAX_BACKUPS; i < backups.size(); ++i) {
                deleteBackup(backups[i].filename);
            }
        }
    }

    std::string formatTimestamp(const std::chrono::system_clock::time_point& tp) const {
        auto tt = std::chrono::system_clock::to_time_t(tp);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    std::string formatSize(size_t bytes) const {
        static const char* units[] = {"B", "KB", "MB", "GB"};
        int unit = 0;
        double size = bytes;
        while (size >= 1024 && unit < 3) {
            size /= 1024;
            unit++;
        }
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << size << " " << units[unit];
        return ss.str();
    }

    void addError(const std::string& error) const {
        errorLog.push_back(error);
        if (errorLog.size() > MAX_ERROR_LOG) {
            errorLog.erase(errorLog.begin());
        }
    }
};

} // namespace test
} // namespace meshtastic

// Helper macros
#define CREATE_BACKUP(config, version, desc) \
    meshtastic::test::ConfigBackup::instance().createBackup(config, version, desc)

#define RESTORE_BACKUP(filename) \
    meshtastic::test::ConfigBackup::instance().restoreBackup(filename)

#define LIST_BACKUPS() \
    meshtastic::test::ConfigBackup::instance().listBackups()

#define GET_BACKUP_REPORT() \
    meshtastic::test::ConfigBackup::instance().getReport()