#pragma once

#include "../mesh/SinglePortPlugin.h"
#include "../mesh/concurrency/OSThread.h"
#include "../mesh/Observer.h"
#include "../mesh/generated/meshtastic/time_window.pb.h"

/**
 * Plugin to manage time-based radio operation windows.
 * Controls when the radio is allowed to transmit based on configured time windows.
 */
class TimeWindowPlugin : public SinglePortPlugin, public concurrency::OSThread
{
public:
    TimeWindowPlugin();

    /** Called every few seconds by the OSThread system */
    virtual int32_t runOnce() override;

    /** Handle incoming protocol messages */
    virtual bool handleReceivedProtobuf(const meshtastic_MeshPacket &mp, meshtastic_TimeWindow *decoded);

    /** Check if current time is within allowed window */
    bool isTimeInWindow(int hour, int minute);

    /** Get current window state */
    bool getCurrentWindowState() const { return isWindowActive && (!temporaryOverride || overrideActive); }

    /** Record statistics */
    void recordQueueStats(uint32_t queueTime);
    void recordDroppedPacket();
    void recordQueueOverflow();
    void updateQueuedCount(size_t count);

protected:
    /** Calculate seconds until next window state transition */
    int32_t calculateNextTransition(int currentHour, int currentMinute);

    /** Get Unix timestamp of next transition */
    uint32_t getNextTransitionTime();

    /** Send status to requesting node */
    void sendStatus(NodeNum dest);

    /** Send statistics to requesting node */
    void sendStats(NodeNum dest);

    /** Handle incoming command */
    void handleCommand(NodeNum from, const meshtastic_TimeWindowCommand &cmd);

private:
    /** Current state */
    bool isWindowActive = true;

    /** Temporary override state */
    bool temporaryOverride = false;
    bool overrideActive = false;
    uint32_t overrideExpiry = 0;

    /** Observable for window state changes */
    Observable<bool> onWindowStateChange;
};

extern TimeWindowPlugin *timeWindowPlugin;