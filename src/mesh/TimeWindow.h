#pragma once

#include "MeshTypes.h"
#include "configuration.h"

namespace meshtastic {

// Controls how packets are handled outside the allowed time window
enum class TimeWindowMode {
    DROP_PACKETS = 0,   // Drop packets when outside the time window
    QUEUE_PACKETS = 1,  // Queue packets when outside the time window
    RECEIVE_ONLY = 2    // Only disable transmit outside window, still allow receive
};

struct TimeWindowConfig {
    bool enabled = false;
    uint8_t start_hour = 21;    // 9 PM default
    uint8_t start_minute = 0;
    uint8_t end_hour = 23;      // 11 PM default
    uint8_t end_minute = 0;
    TimeWindowMode window_mode = TimeWindowMode::RECEIVE_ONLY;
    uint16_t max_queue_size = 32;
    uint32_t packet_expiry_secs = 3600; // 1 hour default
};

struct QueuedPacket {
    meshtastic_MeshPacket* packet;
    uint32_t enqueue_time;
    const TimeWindowConfig* config;
    
    QueuedPacket(meshtastic_MeshPacket* p, uint32_t time, const TimeWindowConfig* cfg)
        : packet(p), enqueue_time(time), config(cfg) {}
    
    bool isExpired(uint32_t now) const {
        return now - enqueue_time >= config->packet_expiry_secs * 1000;
    }
};

// Function declarations
bool isTimeInWindow(const TimeWindowConfig& cfg, int hour, int minute);
bool isTimeWindowActive(const TimeWindowConfig& cfg, uint32_t currentTime);

} // namespace meshtastic