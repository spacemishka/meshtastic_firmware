#include "RadioInterface.h"
#include "configuration.h"
#include "mesh/MeshTypes.h"

struct PacketQueueEntry {
    meshtastic_MeshPacket* packet;
    uint32_t enqueue_time;
};

bool RadioInterface::isOperationAllowed() {
    if (!config.has_lora || !config.lora.time_window_enabled) {
        return true; // If time window is not enabled, always allow operation
    }

    // Convert current time to hours and minutes
    uint32_t now = millis() / 1000; // Current time in seconds
    uint32_t daySeconds = now % (24 * 3600);
    int currentHour = (daySeconds / 3600);
    int currentMinute = (daySeconds % 3600) / 60;
    
    // Convert times to minutes since midnight
    int currentTime = currentHour * 60 + currentMinute;
    int startTime = config.lora.window_start_hour * 60 + config.lora.window_start_minute;
    int endTime = config.lora.window_end_hour * 60 + config.lora.window_end_minute;
    
    bool isInWindow;
    if (startTime <= endTime) {
        // Simple case: window within same day
        isInWindow = (currentTime >= startTime) && (currentTime < endTime);
    } else {
        // Window spans midnight
        isInWindow = (currentTime >= startTime) || (currentTime < endTime);
    }

    return isInWindow;
}

void RadioInterface::cleanExpiredPackets() {
    if (!config.has_lora || !config.lora.time_window_enabled) {
        return;
    }

    uint32_t now = millis();
    auto it = packetQueue.begin();
    
    while (it != packetQueue.end()) {
        uint32_t age = now - it->enqueue_time;
        if (age >= (config.lora.window_packet_expire_secs * 1000)) {
            LOG_DEBUG("Dropping expired packet from queue (age: %ums)", age);
            packetPool.release(it->packet);
            it = packetQueue.erase(it);
        } else {
            ++it;
        }
    }
}

void RadioInterface::processQueuedPackets() {
    if (!config.has_lora || !config.lora.time_window_enabled || !isOperationAllowed()) {
        return;
    }

    cleanExpiredPackets();

    while (!packetQueue.empty()) {
        auto& qp = packetQueue.front();
        ErrorCode result = RadioInterface::send(qp.packet);
        
        if (result == ERRNO_OK) {
            LOG_DEBUG("Sent queued packet successfully");
            packetQueue.erase(packetQueue.begin());
        } else {
            LOG_WARN("Failed to send queued packet, will retry later");
            break;
        }
    }
}

ErrorCode RadioInterface::send(meshtastic_MeshPacket *p) {
    if (config.has_lora && config.lora.time_window_enabled) {
        if (!isOperationAllowed()) {
            switch (config.lora.window_mode) {
                case meshtastic_TimeWindowMode_DROP_PACKETS:
                    LOG_DEBUG("Dropping packet - outside time window");
                    packetPool.release(p);
                    return ERRNO_NO_RADIO;

                case meshtastic_TimeWindowMode_QUEUE_PACKETS:
                    if (packetQueue.size() >= config.lora.window_queue_size) {
                        LOG_DEBUG("Queue full - dropping packet");
                        packetPool.release(p);
                        return ERRNO_NO_RADIO;
                    }
                    
                    PacketQueueEntry entry = {
                        .packet = p,
                        .enqueue_time = millis()
                    };
                    
                    LOG_DEBUG("Queuing packet - outside time window");
                    packetQueue.push_back(entry);
                    return ERRNO_OK;

                case meshtastic_TimeWindowMode_RECEIVE_ONLY:
                    LOG_DEBUG("Dropping TX packet - in receive-only window");
                    packetPool.release(p);
                    return ERRNO_NO_RADIO;

                default:
                    LOG_ERROR("Invalid time window mode");
                    return ERRNO_INVALID_CONFIG;
            }
        }
        
        // Process any queued packets first when we're back in the allowed window
        processQueuedPackets();
    }

    // Now send the current packet
    return RadioInterface::send(p);
}