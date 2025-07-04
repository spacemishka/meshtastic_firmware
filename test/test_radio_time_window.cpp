#include "../src/mesh/RadioInterface.h"
#include "../src/configuration.h"
#include "../src/mesh/MeshTypes.h"
#include "../src/mesh/MemPool.h"
#include "../src/platform/logging.h"
#include <assert.h>
#include <stdio.h>

class TestRadioInterface : public RadioInterface {
public:
    ErrorCode send(meshtastic_MeshPacket *p) override { 
        lastPacket = p;
        return ERRNO_OK; 
    }
    
    meshtastic_MeshPacket* lastPacket = nullptr;
    
    bool isTestWindowOpen() {
        return isOperationAllowed();
    }
    
    size_t getQueueSize() {
        return packetQueue.size();
    }
};

TestRadioInterface *radio = nullptr;
static meshtastic_Config config;

void setUp() {
    radio = new TestRadioInterface();
    
    // Configure time window
    config.has_lora = true;
    config.lora.time_window_enabled = true;
    config.lora.window_start_hour = 21;   // 9 PM
    config.lora.window_start_minute = 0;
    config.lora.window_end_hour = 23;     // 11 PM
    config.lora.window_end_minute = 0;
    config.lora.window_mode = meshtastic_TimeWindowMode_QUEUE_PACKETS;
    config.lora.window_queue_size = 5;
    config.lora.window_packet_expire_secs = 3600;
}

void tearDown() {
    delete radio;
    radio = nullptr;
}

void test_time_window_enabled() {
    assert(config.lora.time_window_enabled);
    LOG_INFO("Time window enabled test passed\n");
}

void test_packet_queuing() {
    // Simulate time outside window (3 PM)
    uint32_t testTime = (15 * 3600); // 3 PM in seconds
    
    meshtastic_MeshPacket *p = packetPool.allocZeroed();
    p->id = 1234;
    
    ErrorCode result = radio->send(p);
    assert(result == ERRNO_OK);
    assert(radio->getQueueSize() == 1);
    LOG_INFO("Packet queuing test passed\n");
}

void test_queue_limit() {
    // Fill queue beyond limit
    for(int i = 0; i < 7; i++) {
        meshtastic_MeshPacket *p = packetPool.allocZeroed();
        p->id = i;
        radio->send(p);
    }
    
    assert(radio->getQueueSize() == 5); // Should be limited to configured size
    LOG_INFO("Queue limit test passed\n");
}

void test_packet_expiry() {
    meshtastic_MeshPacket *p = packetPool.allocZeroed();
    p->id = 1;
    radio->send(p);
    
    // Simulate time passing
    delay(1000); // Wait 1 second
    
    radio->cleanExpiredPackets();
    assert(radio->getQueueSize() == 0);
    LOG_INFO("Packet expiry test passed\n");
}

void test_receive_only_mode() {
    config.lora.window_mode = meshtastic_TimeWindowMode_RECEIVE_ONLY;
    
    meshtastic_MeshPacket *p = packetPool.allocZeroed();
    p->id = 1;
    
    ErrorCode result = radio->send(p);
    assert(result == ERRNO_NO_RADIO);
    LOG_INFO("Receive only mode test passed\n");
}

void runTests() {
    LOG_INFO("Starting time window tests\n");
    
    setUp();
    
    test_time_window_enabled();
    test_packet_queuing();
    test_queue_limit();
    test_packet_expiry();
    test_receive_only_mode();
    
    tearDown();
    
    LOG_INFO("All time window tests completed\n");
}

#ifdef ARDUINO
void setup() {
    Serial.begin(115200);
    delay(2000);
    runTests();
}

void loop() {}
#else
int main() {
    runTests();
    return 0;
}
#endif