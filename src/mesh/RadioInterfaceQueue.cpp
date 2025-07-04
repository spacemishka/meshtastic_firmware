#include "RadioInterface.h"
#include "PriorityPacketQueue.h"
#include "configuration.h"
#include "NodeDB.h"
#include "mesh/generated/meshtastic/time_window.pb.h"

namespace {
    // Single instance of priority queue for radio interface
    meshtastic::PriorityPacketQueue priorityQueue;

    // Queue monitoring
    struct QueueMetrics {
        uint32_t lastProcessTime = 0;    // Last time queue was processed
        uint32_t highPriorityCount = 0;  // High priority packets in last window
        uint32_t normalPriorityCount = 0; // Normal priority packets in last window
        uint32_t dropCount = 0;          // Dropped packets count
        
        void reset() {
            highPriorityCount = 0;
            normalPriorityCount = 0;
            dropCount = 0;
        }
    } metrics;
}

ErrorCode RadioInterface::send(meshtastic_MeshPacket *p) {
    // Time window check
    if (config.has_lora && config.lora.time_window_enabled && !isOperationAllowed()) {
        switch (config.lora.window_mode) {
            case meshtastic_TimeWindowMode_DROP_PACKETS:
                LOG_DEBUG("Dropping packet - outside time window\n");
                metrics.dropCount++;
                packetPool.release(p);
                return ERRNO_NO_RADIO;

            case meshtastic_TimeWindowMode_QUEUE_PACKETS:
                if (!priorityQueue.enqueue(p)) {
                    LOG_DEBUG("Queue full - dropping packet\n");
                    metrics.dropCount++;
                    packetPool.release(p);
                    return ERRNO_NO_RADIO;
                }
                LOG_DEBUG("Packet queued - outside time window (priority: %d)\n", 
                    priorityQueue.getLastPriority());
                return ERRNO_OK;

            case meshtastic_TimeWindowMode_RECEIVE_ONLY:
                LOG_DEBUG("Dropping TX packet - in receive-only window\n");
                metrics.dropCount++;
                packetPool.release(p);
                return ERRNO_NO_RADIO;

            default:
                LOG_ERROR("Invalid time window mode\n");
                return ERRNO_INVALID_CONFIG;
        }
    }

    // Process queued packets if enough time has passed
    uint32_t now = millis();
    if (now - metrics.lastProcessTime >= MIN_QUEUE_PROCESS_INTERVAL) {
        processQueuedPackets();
        metrics.lastProcessTime = now;
    }

    // Send current packet
    return sendPacket(p);
}

void RadioInterface::processQueuedPackets() {
    if (!config.has_lora || !config.lora.time_window_enabled) {
        return;
    }

    // Clean expired packets
    priorityQueue.cleanExpired();

    // Process queue with fairness
    const uint32_t startTime = millis();
    const uint32_t maxProcessTime = 100; // Max 100ms per processing cycle
    uint32_t packetsProcessed = 0;

    while (!priorityQueue.empty() && 
           (millis() - startTime) < maxProcessTime && 
           packetsProcessed < MAX_PACKETS_PER_CYCLE) {
        
        auto packet = priorityQueue.dequeue();
        if (packet) {
            bool isHighPriority = priorityQueue.getLastPriority() > 2;
            
            ErrorCode result = sendPacket(packet);
            if (result == ERRNO_OK) {
                if (isHighPriority)
                    metrics.highPriorityCount++;
                else
                    metrics.normalPriorityCount++;
                packetsProcessed++;
            } else {
                // On failure, try to requeue unless queue is full
                if (!priorityQueue.enqueue(packet)) {
                    LOG_WARN("Failed to requeue packet - dropping\n");
                    metrics.dropCount++;
                    packetPool.release(packet);
                }
                break; // Stop processing on send failure
            }
        }
    }
}

void RadioInterface::clearPacketQueue() {
    priorityQueue.clear();
    metrics.reset();
}

size_t RadioInterface::getQueueSize() const {
    return priorityQueue.size();
}

bool RadioInterface::isQueueFull() const {
    return priorityQueue.full();
}

meshtastic::PriorityPacketQueue::QueueStats RadioInterface::getQueueStats() const {
    return priorityQueue.stats;
}

RadioInterface::QueueMetrics RadioInterface::getQueueMetrics() const {
    return {
        .highPriorityCount = metrics.highPriorityCount,
        .normalPriorityCount = metrics.normalPriorityCount,
        .dropCount = metrics.dropCount,
        .avgQueueTime = priorityQueue.getAvgQueueTime()
    };
}