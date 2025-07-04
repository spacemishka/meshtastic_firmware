#pragma once

#include "MeshTypes.h"
#include "configuration.h"
#include <queue>
#include <vector>
#include <functional>

namespace meshtastic {

/**
 * Priority-based packet queue for time window operation
 */
class PriorityPacketQueue {
public:
    struct QueuedPacket {
        meshtastic_MeshPacket* packet;
        uint32_t enqueue_time;
        uint8_t priority;

        QueuedPacket(meshtastic_MeshPacket* p, uint32_t time) 
            : packet(p), enqueue_time(time) {
            // Calculate priority based on packet type and content
            priority = calculatePriority(p);
        }

        // Compare operator for priority queue
        bool operator<(const QueuedPacket& other) const {
            return priority < other.priority;
        }

    private:
        static uint8_t calculatePriority(const meshtastic_MeshPacket* p) {
            if (!p) return 0;

            // Higher number = higher priority
            uint8_t priority = 1;

            // Priority based on packet type
            if (p->want_ack)
                priority += 2;  // ACK-required packets get higher priority

            // Check for emergency/priority flags in the payload
            if (p->priority == meshtastic_MeshPacket_Priority_RELIABLE)
                priority += 3;
            else if (p->priority == meshtastic_MeshPacket_Priority_ACK)
                priority += 2;

            // Additional priority for specific packet types
            switch (p->which_payload_variant) {
                case meshtastic_MeshPacket_decoded_tag:
                    // Position updates from trackers
                    if (p->decoded.portnum == meshtastic_PortNum_POSITION_APP)
                        priority += 1;
                    // Emergency messages
                    if (p->decoded.portnum == meshtastic_PortNum_EMERGENCY_APP)
                        priority += 4;
                    break;
            }

            return priority;
        }
    };

    PriorityPacketQueue(size_t maxSize = 32, uint32_t expirySeconds = 3600)
        : maxQueueSize(maxSize), packetExpirySeconds(expirySeconds) {}

    bool enqueue(meshtastic_MeshPacket* packet) {
        if (packets.size() >= maxQueueSize) {
            stats.queueOverflows++;
            return false;
        }

        packets.push(QueuedPacket(packet, millis()));
        stats.totalQueued++;
        return true;
    }

    meshtastic_MeshPacket* dequeue() {
        if (packets.empty()) {
            return nullptr;
        }

        const auto& top = packets.top();
        auto packet = top.packet;
        
        uint32_t queueTime = (millis() - top.enqueue_time) / 1000;
        stats.totalQueueTime += queueTime;
        stats.maxQueueTime = std::max(stats.maxQueueTime, queueTime);
        
        packets.pop();
        return packet;
    }

    void cleanExpired() {
        std::priority_queue<QueuedPacket> temp;
        uint32_t now = millis();
        
        while (!packets.empty()) {
            const auto& top = packets.top();
            if ((now - top.enqueue_time) >= (packetExpirySeconds * 1000)) {
                stats.expiredPackets++;
                packetPool.release(top.packet);
            } else {
                temp.push(top);
            }
            packets.pop();
        }
        
        packets = std::move(temp);
    }

    void clear() {
        while (!packets.empty()) {
            packetPool.release(packets.top().packet);
            packets.pop();
        }
    }

    // Statistics structure
    struct QueueStats {
        uint32_t totalQueued = 0;
        uint32_t expiredPackets = 0;
        uint32_t queueOverflows = 0;
        uint32_t totalQueueTime = 0;
        uint32_t maxQueueTime = 0;
        
        void reset() { memset(this, 0, sizeof(*this)); }
    } stats;

    // Configuration
    const size_t maxQueueSize;
    const uint32_t packetExpirySeconds;

    // Accessors
    size_t size() const { return packets.size(); }
    bool empty() const { return packets.empty(); }
    bool full() const { return packets.size() >= maxQueueSize; }
    
    uint32_t getAvgQueueTime() const {
        return stats.totalQueued ? (stats.totalQueueTime / stats.totalQueued) : 0;
    }

private:
    std::priority_queue<QueuedPacket> packets;
};

} // namespace meshtastic