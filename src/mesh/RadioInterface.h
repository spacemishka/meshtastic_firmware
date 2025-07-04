#pragma once

#include "MemoryPool.h"
#include "MeshTypes.h"
#include "Observer.h"
#include "PointerQueue.h"
#include "airtime.h"
#include "error.h"
#include <vector>

// Keep existing defines
#define MAX_TX_QUEUE 16
#define MAX_LORA_PAYLOAD_LEN 255
#define MESHTASTIC_HEADER_LENGTH 16
#define MESHTASTIC_PKC_OVERHEAD 12

#define PACKET_FLAGS_HOP_LIMIT_MASK 0x07
#define PACKET_FLAGS_WANT_ACK_MASK 0x08
#define PACKET_FLAGS_VIA_MQTT_MASK 0x10
#define PACKET_FLAGS_HOP_START_MASK 0xE0
#define PACKET_FLAGS_HOP_START_SHIFT 5

// Keep existing PacketHeader struct
typedef struct {
    NodeNum to, from;
    PacketId id;
    uint8_t flags;
    uint8_t channel;
    uint8_t next_hop;
    uint8_t relay_node;
} PacketHeader;

// Keep existing RadioBuffer struct
typedef struct {
    PacketHeader header;
    uint8_t payload[MAX_LORA_PAYLOAD_LEN + 1 - sizeof(PacketHeader)] __attribute__((__aligned__));
} RadioBuffer;

// Add packet queue entry struct
struct PacketQueueEntry {
    meshtastic_MeshPacket* packet;
    uint32_t enqueue_time;
};

class RadioInterface {
    friend class MeshRadio;

protected:
    // Existing protected members
    bool disabled = false;
    float bw = 125;
    uint8_t sf = 9;
    uint8_t cr = 5;

    // Keep other existing protected members...
    const uint8_t NUM_SYM_CAD = 2;
    const uint8_t NUM_SYM_CAD_24GHZ = 4;
    uint32_t slotTimeMsec = computeSlotTimeMsec();
    uint16_t preambleLength = 16;
    uint32_t preambleTimeMsec = 165;
    uint32_t maxPacketTimeMsec = 3246;
    const uint32_t PROCESSING_TIME_MSEC = 4500;
    const uint8_t CWmin = 3;
    const uint8_t CWmax = 8;

    meshtastic_MeshPacket *sendingPacket = NULL;
    uint32_t lastTxStart = 0L;

    // Add packet queue for time window feature
    std::vector<PacketQueueEntry> packetQueue;

    // Time window helper functions
    bool isOperationAllowed();
    void processQueuedPackets();
    void cleanExpiredPackets();

    RadioBuffer radioBuffer __attribute__((__aligned__));
    void deliverToReceiver(meshtastic_MeshPacket *p);

public:
    // Keep existing public interface
    RadioInterface();
    virtual ~RadioInterface() {}

    virtual bool canSleep() { return packetQueue.empty() && true; }
    virtual bool wideLora() { return false; }
    virtual bool sleep() { return true; }
    void disable() {
        disabled = true;
        sleep();
    }

    // Modified send method to handle time windows
    virtual ErrorCode send(meshtastic_MeshPacket *p);

    // Keep all other existing public methods...
    virtual meshtastic_QueueStatus getQueueStatus();
    virtual bool cancelSending(NodeNum from, PacketId id) { return false; }
    virtual bool findInTxQueue(NodeNum from, PacketId id) { return false; }
    virtual bool init();
    virtual bool reconfigure();

protected:
    // Keep existing protected members
    int8_t power = 17;
    float savedFreq;
    uint32_t savedChannelNum;

    // Keep existing protected methods
    size_t beginSending(meshtastic_MeshPacket *p);
    void limitPower(int8_t MAX_POWER);
    virtual void saveFreq(float savedFreq);
    virtual void saveChannelNum(uint32_t savedChannelNum);

private:
    // Keep existing private methods
    void applyModemConfig();
    int preflightSleepCb(void *unused = NULL) { return canSleep() ? 0 : 1; }
    int notifyDeepSleepCb(void *unused = NULL);
    int reloadConfig(void *unused) {
        reconfigure();
        return 0;
    }
};

void printPacket(const char *prefix, const meshtastic_MeshPacket *p);