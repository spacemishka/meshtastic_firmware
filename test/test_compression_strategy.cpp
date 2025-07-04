#include <gtest/gtest.h>
#include "test_config_compression_strategy.h"
#include <random>
#include <sstream>

using namespace meshtastic::test;

class CompressionStrategyTest : public ::testing::Test {
protected:
    std::string generateTestData(size_t size, bool repeating = false) {
        std::stringstream ss;
        std::random_device rd;
        std::mt19937 gen(rd());
        
        if (repeating) {
            // Generate repeating pattern data
            std::string pattern = "HelloMeshtastic";
            while (ss.str().length() < size) {
                ss << pattern;
            }
        } else {
            // Generate random data
            std::uniform_int_distribution<> dist(32, 126); // printable ASCII
            for (size_t i = 0; i < size; i++) {
                ss << static_cast<char>(dist(gen));
            }
        }
        
        return ss.str().substr(0, size);
    }

    void verifyCompression(CompressionStrategy& strategy, 
                          const std::string& input,
                          double expectedMaxRatio = 1.0) {
        auto compressed = strategy.compress(input);
        auto decompressed = strategy.decompress(compressed);
        
        // Verify correctness
        EXPECT_EQ(input, decompressed) << "Data corrupted by " << strategy.name();
        
        // Verify compression ratio
        double ratio = static_cast<double>(compressed.data.size()) / input.size();
        EXPECT_LE(ratio, expectedMaxRatio) 
            << strategy.name() << " compression ratio too high";
    }
};

TEST_F(CompressionStrategyTest, HuffmanCompression) {
    auto& manager = CompressionManager::instance();
    manager.setAlgorithm(CompressionManager::Algorithm::HUFFMAN);

    // Test with different data types
    std::vector<std::pair<std::string, double>> testCases = {
        {generateTestData(1000, true), 0.6},  // Repeating data
        {generateTestData(1000, false), 0.9}, // Random data
        {"", 1.0},                           // Empty string
        {"AAAAAAAAAA", 0.3},                // Single character
        {"Hello, Meshtastic!", 0.8}         // Normal text
    };

    for (const auto& [data, expectedRatio] : testCases) {
        verifyCompression(*manager.getCurrentStrategy(), data, expectedRatio);
    }
}

TEST_F(CompressionStrategyTest, RLECompression) {
    auto& manager = CompressionManager::instance();
    manager.setAlgorithm(CompressionManager::Algorithm::RLE);

    // Test with different data types
    std::vector<std::pair<std::string, double>> testCases = {
        {std::string(100, 'A'), 0.1},       // Highly compressible
        {generateTestData(1000, false), 1.2}, // Random data (might expand)
        {"", 1.0},                           // Empty string
        {"AABBCCDDEE", 1.0},                // Alternating pairs
        {std::string("ABC") + std::string(50, 'D') + "EFG", 0.3} // Mixed
    };

    for (const auto& [data, expectedRatio] : testCases) {
        verifyCompression(*manager.getCurrentStrategy(), data, expectedRatio);
    }
}

TEST_F(CompressionStrategyTest, LZ77Compression) {
    auto& manager = CompressionManager::instance();
    
    // Test different compression levels
    std::vector<int> levels = {1, 2, 3};
    std::string testData = generateTestData(10000, true); // Large repeating data
    
    double previousRatio = 1.0;
    for (int level : levels) {
        manager.setAlgorithm(CompressionManager::Algorithm::LZ77, level);
        auto compressed = manager.getCurrentStrategy()->compress(testData);
        double ratio = static_cast<double>(compressed.data.size()) / testData.size();
        
        // Higher levels should give better compression
        EXPECT_LE(ratio, previousRatio) 
            << "Level " << level << " compression worse than level " << (level-1);
        previousRatio = ratio;
    }
}

TEST_F(CompressionStrategyTest, CompressionLevels) {
    auto& manager = CompressionManager::instance();
    std::string testData = generateTestData(5000, true);

    struct TestCase {
        CompressionManager::Algorithm algo;
        std::string name;
    };

    std::vector<TestCase> algorithms = {
        {CompressionManager::Algorithm::HUFFMAN, "Huffman"},
        {CompressionManager::Algorithm::RLE, "RLE"},
        {CompressionManager::Algorithm::LZ77, "LZ77"}
    };

    for (const auto& algo : algorithms) {
        std::vector<double> ratios;
        
        for (int level = 1; level <= 3; level++) {
            manager.setAlgorithm(algo.algo, level);
            auto compressed = manager.getCurrentStrategy()->compress(testData);
            double ratio = static_cast<double>(compressed.data.size()) / testData.size();
            ratios.push_back(ratio);
            
            // Verify decompression
            auto decompressed = manager.getCurrentStrategy()->decompress(compressed);
            EXPECT_EQ(testData, decompressed) 
                << algo.name << " level " << level << " corrupted data";
        }

        // Higher levels should not be worse
        for (size_t i = 1; i < ratios.size(); i++) {
            EXPECT_LE(ratios[i], ratios[i-1]) 
                << algo.name << " level " << (i+1) 
                << " worse than level " << i;
        }
    }
}

TEST_F(CompressionStrategyTest, EdgeCases) {
    auto& manager = CompressionManager::instance();
    
    std::vector<std::string> edgeCases = {
        "",                                    // Empty string
        "A",                                   // Single character
        std::string(1000, 'A'),               // Repeated character
        std::string(1, '\0'),                 // Null character
        std::string("Hello\0World", 11),      // Embedded nulls
        std::string(1000, '\n'),              // All newlines
        generateTestData(1000, false)         // Random data
    };

    std::vector<CompressionManager::Algorithm> algorithms = {
        CompressionManager::Algorithm::HUFFMAN,
        CompressionManager::Algorithm::RLE,
        CompressionManager::Algorithm::LZ77
    };

    for (auto algo : algorithms) {
        manager.setAlgorithm(algo);
        for (const auto& testCase : edgeCases) {
            verifyCompression(*manager.getCurrentStrategy(), testCase);
        }
    }
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}