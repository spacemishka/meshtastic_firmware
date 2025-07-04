#include "RadioInterface.h"
#include "NodeDB.h"
#include "MeshTypes.h"
#include "configuration.h"
#include "mesh/generated/meshtastic/lora_config.pb.h"
#include "PacketQueue.h"

namespace {
    // Queue instance for time window operation
    meshtastic::PacketQueue packetQueue;
}

ErrorCode RadioInterface::send(meshtastic_MeshPacket *p) {
    // Check if time window is enabled and we're outside the window
    if (config.has_lora && config.lora.time_window_enabled && !isOperationAllowed()) {
        switch (config.lora.window_mode) {
            case meshtastic_TimeWindowMode_DROP_PACKETS:
                LOG_DEBUG("Dropping packet - outside time window\n");
                packetPool.release(p);
                return ERRNO_NO_RADIO;

            case meshtastic_TimeWindowMode_QUEUE_PACKETS:
                if (!packetQueue.enqueue(p)) {
                    LOG_DEBUG("Queue full - dropping packet\n");
                    packetPool.release(p);
                    return ERRNO_NO_RADIO;
                }
                LOG_DEBUG("Packet queued - outside time window\n");
                return ERRNO_OK;

            case meshtastic_TimeWindowMode_RECEIVE_ONLY:
                LOG_DEBUG("Dropping TX packet - in receive-only window\n");
                packetPool.release(p);
                return ERRNO_NO_RADIO;

            default:
                LOG_ERROR("Invalid time window mode\n");
                return ERRNO_INVALID_CONFIG;
        }
    }

    // Process any queued packets first
    processQueuedPackets();

    // Now handle the current packet
    return sendPacket(p);
}

void RadioInterface::processQueuedPackets() {
    if (!config.has_lora || !config.lora.time_window_enabled) {
        return;
    }

    // Clean expired packets first
    packetQueue.cleanExpired();

    while (!packetQueue.empty()) {
        auto packet = packetQueue.dequeue();
        if (packet) {
            ErrorCode result = sendPacket(packet);
            if (result != ERRNO_OK) {
                // If send failed, try to requeue unless queue is full
                if (!packetQueue.enqueue(packet)) {
                    LOG_WARN("Failed to requeue packet - dropping\n");
                    packetPool.release(packet);
                }
                break; // Stop processing queue if send failed
            }
        }
    }
}

ErrorCode RadioInterface::sendPacket(meshtastic_MeshPacket *p) {
    if (disabled) {
        packetPool.release(p);
        return ERRNO_NO_RADIO;
    }

    // Check packet validity
    if (!p->payloadlen) {
        LOG_WARN("Zero length packet dropped\n");
        packetPool.release(p);
        return ERRNO_INVALID_LENGTH;
    }

    // Initialize radio buffer with packet data
    size_t numbytes = beginSending(p);
    if (!numbytes) {
        return ERRNO_INVALID_LENGTH;
    }

    return sendTo(&radioBuffer.header, numbytes);
}

void RadioInterface::clearPacketQueue() {
    packetQueue.clear();
}

meshtastic::PacketQueue::QueueStats RadioInterface::getQueueStats() const {
    return packetQueue.stats;
}

size_t RadioInterface::getQueueSize() const {
    return packetQueue.size();
}

bool RadioInterface::isQueueFull() const {
    return packetQueue.full();
}

uint32_t RadioInterface::getAvgQueueTime() const {
    return packetQueue.getAvgQueueTime();
}