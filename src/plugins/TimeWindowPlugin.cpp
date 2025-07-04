#include "TimeWindowPlugin.h"
#include "NodeDB.h"
#include "MeshService.h"
#include "configuration.h"
#include "mesh/generated/meshtastic/time_window.pb.h"

TimeWindowPlugin *timeWindowPlugin;

// Statistics counters
static struct {
    uint32_t totalQueued;
    uint32_t totalDropped;
    uint32_t totalDelayed;
    uint32_t queueOverflows;
    uint32_t maxQueueTime;
    uint32_t sumQueueTime;
    uint32_t queuedPackets;
} stats = {};

TimeWindowPlugin::TimeWindowPlugin()
    : SinglePortPlugin("TimeWindow", meshtastic_PortNum_TIME_WINDOW_APP),
      concurrency::OSThread("TimeWindow")
{
    isEnabled = true;
}

bool TimeWindowPlugin::handleReceivedProtobuf(const meshtastic_MeshPacket &mp, meshtastic_TimeWindow *decoded) 
{
    switch (decoded->type) {
        case meshtastic_TimeWindowMessageType_WINDOW_STATUS: {
            // Status request - send current status
            sendStatus(mp.from);
            break;
        }
        
        case meshtastic_TimeWindowMessageType_WINDOW_STATS: {
            // Statistics request - send current stats
            sendStats(mp.from);
            break;
        }
        
        case meshtastic_TimeWindowMessageType_WINDOW_COMMAND: {
            handleCommand(mp.from, decoded->command);
            break;
        }
    }
    
    return true;
}

void TimeWindowPlugin::sendStatus(NodeNum dest) 
{
    meshtastic_TimeWindow status;
    status.type = meshtastic_TimeWindowMessageType_WINDOW_STATUS;
    status.status.is_active = isWindowActive;
    status.status.next_change = getNextTransitionTime();
    status.status.queued_packets = stats.queuedPackets;
    status.status.dropped_packets = stats.totalDropped;
    status.status.window_mode = config.lora.window_mode;
    
    sendPayload(dest, meshtastic_PortNum_TIME_WINDOW_APP, &status);
}

void TimeWindowPlugin::sendStats(NodeNum dest) 
{
    meshtastic_TimeWindow msg;
    msg.type = meshtastic_TimeWindowMessageType_WINDOW_STATS;
    msg.stats.total_queued = stats.totalQueued;
    msg.stats.total_dropped = stats.totalDropped;
    msg.stats.total_delayed = stats.totalDelayed;
    msg.stats.avg_queue_time = stats.queuedPackets ? (stats.sumQueueTime / stats.queuedPackets) : 0;
    msg.stats.max_queue_time = stats.maxQueueTime;
    msg.stats.queue_overflows = stats.queueOverflows;
    
    sendPayload(dest, meshtastic_PortNum_TIME_WINDOW_APP, &msg);
}

void TimeWindowPlugin::handleCommand(NodeNum from, const meshtastic_TimeWindowCommand &cmd) 
{
    switch (cmd.command) {
        case meshtastic_TimeWindowCommand_CommandType_GET_STATUS:
            sendStatus(from);
            break;
            
        case meshtastic_TimeWindowCommand_CommandType_GET_STATS:
            sendStats(from);
            break;
            
        case meshtastic_TimeWindowCommand_CommandType_FORCE_OPEN:
            temporaryOverride = true;
            overrideActive = true;
            overrideExpiry = millis() + (cmd.duration * 1000);
            LOG_INFO("Time window temporarily forced open for %us\n", cmd.duration);
            break;
            
        case meshtastic_TimeWindowCommand_CommandType_FORCE_CLOSE:
            temporaryOverride = true;
            overrideActive = false;
            overrideExpiry = millis() + (cmd.duration * 1000);
            LOG_INFO("Time window temporarily forced closed for %us\n", cmd.duration);
            break;
            
        case meshtastic_TimeWindowCommand_CommandType_RESET_STATS:
            memset(&stats, 0, sizeof(stats));
            LOG_INFO("Time window statistics reset\n");
            break;
            
        case meshtastic_TimeWindowCommand_CommandType_CLEAR_QUEUE:
            if (service.radio) {
                service.radio->clearPacketQueue();
                stats.queuedPackets = 0;
            }
            LOG_INFO("Packet queue cleared\n");
            break;
    }
}

uint32_t TimeWindowPlugin::getNextTransitionTime() 
{
    if (!config.has_lora || !config.lora.time_window_enabled) {
        return 0;
    }
    
    time_t now = getTime();
    struct tm *timeinfo = localtime(&now);
    
    int32_t secs = calculateNextTransition(timeinfo->tm_hour, timeinfo->tm_min);
    return now + secs;
}

void TimeWindowPlugin::recordQueueStats(uint32_t queueTime) 
{
    stats.totalQueued++;
    stats.sumQueueTime += queueTime;
    stats.maxQueueTime = max(stats.maxQueueTime, queueTime);
}

void TimeWindowPlugin::recordDroppedPacket() 
{
    stats.totalDropped++;
}

void TimeWindowPlugin::recordQueueOverflow() 
{
    stats.queueOverflows++;
}

void TimeWindowPlugin::updateQueuedCount(size_t count) 
{
    stats.queuedPackets = count;
}
