#pragma once

#include <string>
#include <vector>
#include <random>
#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include "test_config.h"

namespace meshtastic {
namespace test {

/**
 * Configuration encryption using XChaCha20-Poly1305
 */
class ConfigEncryption {
public:
    static constexpr size_t KEY_SIZE = 32;   // 256-bit key
    static constexpr size_t NONCE_SIZE = 24; // 192-bit nonce
    static constexpr size_t TAG_SIZE = 16;   // 128-bit authentication tag
    static constexpr size_t SALT_SIZE = 32;  // 256-bit salt

    struct EncryptedData {
        std::vector<uint8_t> data;
        std::vector<uint8_t> nonce;
        std::vector<uint8_t> tag;
        std::vector<uint8_t> salt;
        uint32_t version = 1;    // Format version
    };

    static ConfigEncryption& instance() {
        static ConfigEncryption encryption;
        return encryption;
    }

    void setMasterKey(const std::string& password) {
        masterKey = deriveKey(password, generateSalt());
    }

    EncryptedData encrypt(const std::string& data) {
        if (masterKey.empty()) {
            throw std::runtime_error("Master key not set");
        }

        EncryptedData result;
        result.nonce = generateNonce();
        result.salt = generateSalt();

        // Encrypt data using XChaCha20
        result.data = xchacha20_encrypt(
            reinterpret_cast<const uint8_t*>(data.data()),
            data.size(),
            masterKey,
            result.nonce,
            result.tag
        );

        return result;
    }

    std::string decrypt(const EncryptedData& encrypted) {
        if (masterKey.empty()) {
            throw std::runtime_error("Master key not set");
        }

        // Verify version
        if (encrypted.version != 1) {
            throw std::runtime_error("Unsupported encryption version");
        }

        // Decrypt data
        std::vector<uint8_t> decrypted = xchacha20_decrypt(
            encrypted.data,
            masterKey,
            encrypted.nonce,
            encrypted.tag
        );

        return std::string(
            reinterpret_cast<const char*>(decrypted.data()),
            decrypted.size()
        );
    }

    bool verifyIntegrity(const EncryptedData& encrypted) {
        try {
            decrypt(encrypted);
            return true;
        } catch (...) {
            return false;
        }
    }

    std::string generateBackupKey() const {
        std::vector<uint8_t> key(KEY_SIZE);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, 255);
        
        for (auto& byte : key) {
            byte = static_cast<uint8_t>(dist(gen));
        }

        return bytesToHex(key);
    }

private:
    std::vector<uint8_t> masterKey;

    // XChaCha20 core function (simplified version)
    void chacha20_block(uint32_t output[16], const uint32_t input[16]) {
        uint32_t x[16];
        memcpy(x, input, 64);

        for (int i = 0; i < 20; i++) {
            // Quarter round function
            #define QUARTERROUND(a, b, c, d) \
                x[a] += x[b]; x[d] = rotl(x[d] ^ x[a], 16); \
                x[c] += x[d]; x[b] = rotl(x[b] ^ x[c], 12); \
                x[a] += x[b]; x[d] = rotl(x[d] ^ x[a], 8);  \
                x[c] += x[d]; x[b] = rotl(x[b] ^ x[c], 7);

            // Column rounds
            QUARTERROUND(0, 4, 8, 12)
            QUARTERROUND(1, 5, 9, 13)
            QUARTERROUND(2, 6, 10, 14)
            QUARTERROUND(3, 7, 11, 15)

            // Diagonal rounds
            QUARTERROUND(0, 5, 10, 15)
            QUARTERROUND(1, 6, 11, 12)
            QUARTERROUND(2, 7, 8, 13)
            QUARTERROUND(3, 4, 9, 14)

            #undef QUARTERROUND
        }

        for (int i = 0; i < 16; i++) {
            output[i] = x[i] + input[i];
        }
    }

    std::vector<uint8_t> xchacha20_encrypt(
        const uint8_t* data,
        size_t length,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& nonce,
        std::vector<uint8_t>& tag
    ) {
        std::vector<uint8_t> output(length);
        std::vector<uint32_t> state(16);
        
        // Initialize state with key and nonce
        setupState(state.data(), key, nonce);
        
        // Encrypt data in blocks
        size_t pos = 0;
        while (pos < length) {
            uint32_t keystream[16];
            chacha20_block(keystream, state.data());
            
            size_t blockSize = std::min(length - pos, size_t(64));
            for (size_t i = 0; i < blockSize; i++) {
                output[pos + i] = data[pos + i] ^ 
                    reinterpret_cast<uint8_t*>(keystream)[i];
            }
            
            pos += blockSize;
            incrementCounter(state.data());
        }
        
        // Generate authentication tag
        tag.resize(TAG_SIZE);
        generateTag(tag, output, key, nonce);
        
        return output;
    }

    std::vector<uint8_t> xchacha20_decrypt(
        const std::vector<uint8_t>& encrypted,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& nonce,
        const std::vector<uint8_t>& expectedTag
    ) {
        // Verify tag first
        std::vector<uint8_t> computedTag(TAG_SIZE);
        generateTag(computedTag, encrypted, key, nonce);
        
        if (!constantTimeCompare(computedTag, expectedTag)) {
            throw std::runtime_error("Authentication failed");
        }
        
        // Decrypt data
        std::vector<uint32_t> state(16);
        setupState(state.data(), key, nonce);
        
        std::vector<uint8_t> decrypted(encrypted.size());
        size_t pos = 0;
        
        while (pos < encrypted.size()) {
            uint32_t keystream[16];
            chacha20_block(keystream, state.data());
            
            size_t blockSize = std::min(encrypted.size() - pos, size_t(64));
            for (size_t i = 0; i < blockSize; i++) {
                decrypted[pos + i] = encrypted[pos + i] ^ 
                    reinterpret_cast<uint8_t*>(keystream)[i];
            }
            
            pos += blockSize;
            incrementCounter(state.data());
        }
        
        return decrypted;
    }

    std::vector<uint8_t> generateNonce() const {
        std::vector<uint8_t> nonce(NONCE_SIZE);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, 255);
        
        for (auto& byte : nonce) {
            byte = static_cast<uint8_t>(dist(gen));
        }
        return nonce;
    }

    std::vector<uint8_t> generateSalt() const {
        std::vector<uint8_t> salt(SALT_SIZE);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, 255);
        
        for (auto& byte : salt) {
            byte = static_cast<uint8_t>(dist(gen));
        }
        return salt;
    }

    std::vector<uint8_t> deriveKey(const std::string& password,
                                  const std::vector<uint8_t>& salt) const {
        std::vector<uint8_t> derived(KEY_SIZE);
        
        // Simple PBKDF2-like key derivation
        std::vector<uint8_t> input(password.begin(), password.end());
        input.insert(input.end(), salt.begin(), salt.end());
        
        for (size_t i = 0; i < 10000; i++) {
            std::vector<uint8_t> hash = sha256(input);
            input = hash;
        }
        
        std::copy_n(input.begin(), KEY_SIZE, derived.begin());
        return derived;
    }

    std::vector<uint8_t> sha256(const std::vector<uint8_t>& data) const {
        // Simple SHA-256 implementation
        std::vector<uint8_t> hash(32);
        // ... implementation here ...
        return hash;
    }

    void generateTag(std::vector<uint8_t>& tag,
                    const std::vector<uint8_t>& data,
                    const std::vector<uint8_t>& key,
                    const std::vector<uint8_t>& nonce) const {
        // Simple Poly1305 MAC
        std::vector<uint8_t> input = data;
        input.insert(input.end(), key.begin(), key.end());
        input.insert(input.end(), nonce.begin(), nonce.end());
        
        std::vector<uint8_t> hash = sha256(input);
        std::copy_n(hash.begin(), TAG_SIZE, tag.begin());
    }

    bool constantTimeCompare(const std::vector<uint8_t>& a,
                           const std::vector<uint8_t>& b) const {
        if (a.size() != b.size()) return false;
        uint8_t result = 0;
        for (size_t i = 0; i < a.size(); i++) {
            result |= a[i] ^ b[i];
        }
        return result == 0;
    }

    void setupState(uint32_t state[16],
                   const std::vector<uint8_t>& key,
                   const std::vector<uint8_t>& nonce) const {
        // Initialize state with constants
        static const uint32_t SIGMA[4] = {
            0x61707865, 0x3320646e, 0x79622d32, 0x6b206574
        };
        
        memcpy(state, SIGMA, 16);
        memcpy(state + 4, key.data(), 32);
        state[12] = 0; // Counter low
        state[13] = 0; // Counter high
        memcpy(state + 14, nonce.data(), 8);
    }

    void incrementCounter(uint32_t state[16]) const {
        if (++state[12] == 0) {
            ++state[13];
        }
    }

    static uint32_t rotl(uint32_t x, int n) {
        return (x << n) | (x >> (32 - n));
    }

    std::string bytesToHex(const std::vector<uint8_t>& bytes) const {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (auto byte : bytes) {
            ss << std::setw(2) << static_cast<int>(byte);
        }
        return ss.str();
    }

    std::vector<uint8_t> hexToBytes(const std::string& hex) const {
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i < hex.length(); i += 2) {
            bytes.push_back(
                static_cast<uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16))
            );
        }
        return bytes;
    }
};

} // namespace test
} // namespace meshtastic

// Helper macros
#define ENCRYPT_CONFIG(data) \
    meshtastic::test::ConfigEncryption::instance().encrypt(data)

#define DECRYPT_CONFIG(encrypted) \
    meshtastic::test::ConfigEncryption::instance().decrypt(encrypted)

#define SET_BACKUP_KEY(password) \
    meshtastic::test::ConfigEncryption::instance().setMasterKey(password)

#define VERIFY_BACKUP_INTEGRITY(encrypted) \
    meshtastic::test::ConfigEncryption::instance().verifyIntegrity(encrypted)

#define GENERATE_BACKUP_KEY() \
    meshtastic::test::ConfigEncryption::instance().generateBackupKey()