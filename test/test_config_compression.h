#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <memory>
#include <algorithm>

namespace meshtastic {
namespace test {

/**
 * Custom compression for configuration data
 */
class ConfigCompression {
public:
    // Huffman tree node
    struct HuffmanNode {
        char ch;
        size_t frequency;
        std::shared_ptr<HuffmanNode> left, right;
        
        HuffmanNode(char c, size_t f) : ch(c), frequency(f), left(nullptr), right(nullptr) {}
    };

    // Compare nodes for priority queue
    struct CompareNodes {
        bool operator()(const std::shared_ptr<HuffmanNode>& a, 
                       const std::shared_ptr<HuffmanNode>& b) {
            return a->frequency > b->frequency;
        }
    };

    struct CompressedData {
        std::vector<uint8_t> data;
        size_t originalSize;
        std::unordered_map<char, std::string> encoding;
    };

    static ConfigCompression& instance() {
        static ConfigCompression compression;
        return compression;
    }

    CompressedData compress(const std::string& input) {
        CompressedData result;
        result.originalSize = input.size();

        if (input.empty()) {
            return result;
        }

        // Build frequency table
        std::unordered_map<char, size_t> frequencies;
        for (char ch : input) {
            frequencies[ch]++;
        }

        // Build Huffman tree
        auto root = buildHuffmanTree(frequencies);
        
        // Generate encoding table
        std::string code;
        generateEncodingTable(root, code, result.encoding);

        // Encode data
        std::string encodedBits;
        for (char ch : input) {
            encodedBits += result.encoding[ch];
        }

        // Pack bits into bytes
        result.data = packBits(encodedBits);

        return result;
    }

    std::string decompress(const CompressedData& compressed) {
        if (compressed.data.empty()) {
            return "";
        }

        // Rebuild decoding tree
        auto root = rebuildDecodingTree(compressed.encoding);

        // Unpack bits
        std::string bits = unpackBits(compressed.data);

        // Decode data
        std::string result;
        result.reserve(compressed.originalSize);
        
        auto current = root;
        for (char bit : bits) {
            if (bit == '0') {
                current = current->left;
            } else {
                current = current->right;
            }

            if (!current->left && !current->right) {
                result += current->ch;
                current = root;
            }

            if (result.size() >= compressed.originalSize) {
                break;
            }
        }

        return result;
    }

    double getCompressionRatio(const CompressedData& compressed) const {
        if (compressed.originalSize == 0) return 1.0;
        return static_cast<double>(compressed.data.size()) / compressed.originalSize;
    }

private:
    std::shared_ptr<HuffmanNode> buildHuffmanTree(
        const std::unordered_map<char, size_t>& frequencies) {
        
        std::priority_queue<
            std::shared_ptr<HuffmanNode>,
            std::vector<std::shared_ptr<HuffmanNode>>,
            CompareNodes
        > pq;

        // Create leaf nodes
        for (const auto& [ch, freq] : frequencies) {
            pq.push(std::make_shared<HuffmanNode>(ch, freq));
        }

        // Build tree
        while (pq.size() > 1) {
            auto left = pq.top(); pq.pop();
            auto right = pq.top(); pq.pop();

            auto parent = std::make_shared<HuffmanNode>('\0', 
                left->frequency + right->frequency);
            parent->left = left;
            parent->right = right;

            pq.push(parent);
        }

        return pq.top();
    }

    void generateEncodingTable(
        const std::shared_ptr<HuffmanNode>& node,
        std::string code,
        std::unordered_map<char, std::string>& encoding) {
        
        if (!node) return;

        if (!node->left && !node->right) {
            encoding[node->ch] = code;
            return;
        }

        generateEncodingTable(node->left, code + "0", encoding);
        generateEncodingTable(node->right, code + "1", encoding);
    }

    std::vector<uint8_t> packBits(const std::string& bits) {
        std::vector<uint8_t> result;
        result.reserve((bits.size() + 7) / 8);

        for (size_t i = 0; i < bits.size(); i += 8) {
            uint8_t byte = 0;
            for (size_t j = 0; j < 8 && i + j < bits.size(); j++) {
                if (bits[i + j] == '1') {
                    byte |= (1 << (7 - j));
                }
            }
            result.push_back(byte);
        }

        return result;
    }

    std::string unpackBits(const std::vector<uint8_t>& bytes) {
        std::string result;
        result.reserve(bytes.size() * 8);

        for (uint8_t byte : bytes) {
            for (int i = 7; i >= 0; i--) {
                result += ((byte & (1 << i)) ? '1' : '0');
            }
        }

        return result;
    }

    std::shared_ptr<HuffmanNode> rebuildDecodingTree(
        const std::unordered_map<char, std::string>& encoding) {
        
        auto root = std::make_shared<HuffmanNode>('\0', 0);

        for (const auto& [ch, code] : encoding) {
            auto current = root;
            for (char bit : code) {
                if (bit == '0') {
                    if (!current->left) {
                        current->left = std::make_shared<HuffmanNode>('\0', 0);
                    }
                    current = current->left;
                } else {
                    if (!current->right) {
                        current->right = std::make_shared<HuffmanNode>('\0', 0);
                    }
                    current = current->right;
                }
            }
            current->ch = ch;
        }

        return root;
    }
};

} // namespace test
} // namespace meshtastic

// Helper macros for compression
#define COMPRESS_CONFIG(data) \
    meshtastic::test::ConfigCompression::instance().compress(data)

#define DECOMPRESS_CONFIG(compressed) \
    meshtastic::test::ConfigCompression::instance().decompress(compressed)

#define GET_COMPRESSION_RATIO(compressed) \
    meshtastic::test::ConfigCompression::instance().getCompressionRatio(compressed)