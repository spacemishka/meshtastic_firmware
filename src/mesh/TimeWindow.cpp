#include "TimeWindow.h"
#include "configuration.h"
#include <Arduino.h>

namespace meshtastic {

static bool isTimeInWindow(const TimeWindowConfig& cfg, int hour, int minute) {
    // Convert times to minutes for easier comparison
    int current = hour * 60 + minute;
    int start = cfg.start_hour * 60 + cfg.start_minute;
    int end = cfg.end_hour * 60 + cfg.end_minute;
    
    if (start <= end) {
        // Simple case: window is within same day
        return (current >= start) && (current < end);
    } else {
        // Window spans midnight
        return (current >= start) || (current < end);
    }
}

bool isTimeWindowActive(const TimeWindowConfig& cfg) {
    if (!cfg.enabled) {
        return true; // If time window is disabled, always allow operation
    }

    // Get current hour and minute from system time
    unsigned long currentMillis = millis();
    unsigned long currentSeconds = currentMillis / 1000;
    int currentHour = (currentSeconds / 3600) % 24;
    int currentMinute = (currentSeconds / 60) % 60;

    return isTimeInWindow(cfg, currentHour, currentMinute);
}

} // namespace meshtastic