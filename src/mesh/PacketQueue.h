#pragma once

#include "MeshTypes.h"
#include "configuration.h"
#include <vector>
#include <functional>

namespace meshtastic {

/**
 * Queue for managing packets during time window operation
 */
class PacketQueue {
public:
    struct QueuedPacket {
        meshtastic_MeshPacket* packet;
        uint32_t enqueue_time;
        
        QueuedPacket(meshtastic_MeshPacket* p, uint32_t time) 
            : packet(p), enqueue_time(time) {}
    };

    PacketQueue(size_t maxSize = 32, uint32_t expirySeconds = 3600)
        : maxQueueSize(maxSize), packetExpirySeconds(expirySeconds) {}

    /**
     * Add packet to queue
     * @return true if queued successfully, false if queue full
     */
    bool enqueue(meshtastic_MeshPacket* packet) {
        if (packets.size() >= maxQueueSize) {
            stats.queueOverflows++;
            return false;
        }

        packets.emplace_back(packet, millis());
        stats.totalQueued++;
        return true;
    }

    /**
     * Get next packet to process
     * @return packet or nullptr if queue empty
     */
    meshtastic_MeshPacket* dequeue() {
        if (packets.empty()) {
            return nullptr;
        }

        auto& front = packets.front();
        auto packet = front.packet;
        
        uint32_t queueTime = (millis() - front.enqueue_time) / 1000;
        stats.totalQueueTime += queueTime;
        stats.maxQueueTime = std::max(stats.maxQueueTime, queueTime);
        
        packets.erase(packets.begin());
        return packet;
    }

    /**
     * Remove expired packets from queue
     */
    void cleanExpired() {
        uint32_t now = millis();
        auto it = packets.begin();
        
        while (it != packets.end()) {
            if ((now - it->enqueue_time) >= (packetExpirySeconds * 1000)) {
                stats.expiredPackets++;
                packetPool.release(it->packet);
                it = packets.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * Clear all packets from queue
     */
    void clear() {
        for (auto& item : packets) {
            packetPool.release(item.packet);
        }
        packets.clear();
    }

    /**
     * Get current queue statistics
     */
    struct QueueStats {
        uint32_t totalQueued = 0;      // Total packets queued
        uint32_t expiredPackets = 0;   // Packets that expired
        uint32_t queueOverflows = 0;   // Times queue was full
        uint32_t totalQueueTime = 0;   // Total time packets spent in queue
        uint32_t maxQueueTime = 0;     // Maximum time any packet spent in queue
        
        void reset() { memset(this, 0, sizeof(*this)); }
    } stats;

    // Queue configuration
    const size_t maxQueueSize;
    const uint32_t packetExpirySeconds;

    // Accessors
    size_t size() const { return packets.size(); }
    bool empty() const { return packets.empty(); }
    bool full() const { return packets.size() >= maxQueueSize; }
    
    /**
     * Get average queue time in seconds
     */
    uint32_t getAvgQueueTime() const {
        return stats.totalQueued ? (stats.totalQueueTime / stats.totalQueued) : 0;
    }

private:
    std::vector<QueuedPacket> packets;
};

} // namespace meshtastic