#include <unity.h>
#include "mesh/RadioInterface.h"
#include "mesh/TimeWindow.h"
#include "plugins/TimeWindowPlugin.h"
#include "mesh/generated/meshtastic/time_window.pb.h"
#include <ctime>

// Mock classes for testing
class MockRadioInterface : public RadioInterface {
public:
    std::vector<meshtastic_MeshPacket*> sentPackets;
    std::vector<meshtastic_MeshPacket*> queuedPackets;
    std::vector<meshtastic_MeshPacket*> droppedPackets;

    ErrorCode send(meshtastic_MeshPacket* p) override {
        sentPackets.push_back(p);
        return ERRNO_OK;
    }

    void clearQueues() {
        sentPackets.clear();
        queuedPackets.clear();
        droppedPackets.clear();
    }
};

class TestTimeWindow {
private:
    MockRadioInterface* radio;
    TimeWindowPlugin* plugin;
    meshtastic_Config originalConfig;

public:
    void setUp() {
        radio = new MockRadioInterface();
        plugin = new TimeWindowPlugin();
        
        // Save original config
        originalConfig = config;
        
        // Setup test config
        config.has_lora = true;
        config.lora.time_window_enabled = true;
        config.lora.window_start_hour = 9;    // 9 AM
        config.lora.window_start_minute = 0;
        config.lora.window_end_hour = 17;     // 5 PM
        config.lora.window_end_minute = 0;
        config.lora.window_mode = meshtastic_TimeWindowMode_QUEUE_PACKETS;
        config.lora.window_queue_size = 5;
        config.lora.window_packet_expire_secs = 3600;
    }

    void tearDown() {
        delete radio;
        delete plugin;
        config = originalConfig;
    }

    void runTests() {
        TEST_ASSERT_TRUE(plugin != nullptr);
        
        testBasicWindowOperation();
        testPacketQueueing();
        testPacketExpiry();
        testQueueOverflow();
        testPriorityHandling();
        testModeChanges();
        testWindowTransitions();
        testTimezoneHandling();
        testStatistics();
    }

private:
    void testBasicWindowOperation() {
        // Test during window hours
        setTestTime(14, 0); // 2 PM
        TEST_ASSERT_TRUE(plugin->isTimeInWindow(14, 0));
        
        // Test outside window hours
        setTestTime(20, 0); // 8 PM
        TEST_ASSERT_FALSE(plugin->isTimeInWindow(20, 0));
    }

    void testPacketQueueing() {
        setTestTime(20, 0); // Outside window
        
        meshtastic_MeshPacket* p1 = createTestPacket(1);
        meshtastic_MeshPacket* p2 = createTestPacket(2);
        
        radio->send(p1);
        radio->send(p2);
        
        TEST_ASSERT_EQUAL(0, radio->sentPackets.size());
        TEST_ASSERT_EQUAL(2, radio->queuedPackets.size());
        
        // Move to window hours
        setTestTime(14, 0);
        plugin->runOnce();
        
        TEST_ASSERT_EQUAL(2, radio->sentPackets.size());
        TEST_ASSERT_EQUAL(0, radio->queuedPackets.size());
    }

    void testPacketExpiry() {
        setTestTime(20, 0);
        meshtastic_MeshPacket* p = createTestPacket(1);
        radio->send(p);
        
        // Advance time past expiry
        setTestTime(20, 0, 3601); // 1 hour + 1 second
        plugin->runOnce();
        
        TEST_ASSERT_EQUAL(0, radio->queuedPackets.size());
        TEST_ASSERT_EQUAL(1, radio->droppedPackets.size());
    }

    void testQueueOverflow() {
        setTestTime(20, 0);
        
        // Queue more packets than queue size
        for (int i = 0; i < 7; i++) {
            radio->send(createTestPacket(i));
        }
        
        TEST_ASSERT_EQUAL(5, radio->queuedPackets.size());
        TEST_ASSERT_EQUAL(2, radio->droppedPackets.size());
    }

    void testPriorityHandling() {
        setTestTime(20, 0);
        
        meshtastic_MeshPacket* high = createTestPacket(1, true);
        meshtastic_MeshPacket* normal = createTestPacket(2, false);
        
        radio->send(normal);
        radio->send(high);
        
        // Move to window hours
        setTestTime(14, 0);
        plugin->runOnce();
        
        // High priority should be sent first
        TEST_ASSERT_EQUAL(high, radio->sentPackets[0]);
        TEST_ASSERT_EQUAL(normal, radio->sentPackets[1]);
    }

    void testModeChanges() {
        setTestTime(20, 0);
        
        // Test DROP_PACKETS mode
        config.lora.window_mode = meshtastic_TimeWindowMode_DROP_PACKETS;
        radio->send(createTestPacket(1));
        TEST_ASSERT_EQUAL(1, radio->droppedPackets.size());
        
        // Test QUEUE_PACKETS mode
        config.lora.window_mode = meshtastic_TimeWindowMode_QUEUE_PACKETS;
        radio->send(createTestPacket(2));
        TEST_ASSERT_EQUAL(1, radio->queuedPackets.size());
        
        // Test RECEIVE_ONLY mode
        config.lora.window_mode = meshtastic_TimeWindowMode_RECEIVE_ONLY;
        radio->send(createTestPacket(3));
        TEST_ASSERT_EQUAL(0, radio->sentPackets.size());
    }

    void testWindowTransitions() {
        // Start outside window
        setTestTime(20, 0);
        radio->send(createTestPacket(1));
        TEST_ASSERT_EQUAL(1, radio->queuedPackets.size());
        
        // Move to window start
        setTestTime(9, 0);
        plugin->runOnce();
        TEST_ASSERT_EQUAL(1, radio->sentPackets.size());
        TEST_ASSERT_EQUAL(0, radio->queuedPackets.size());
    }

    void testTimezoneHandling() {
        // Test across midnight
        config.lora.window_start_hour = 22; // 10 PM
        config.lora.window_end_hour = 4;    // 4 AM
        
        setTestTime(23, 0); // 11 PM
        TEST_ASSERT_TRUE(plugin->isTimeInWindow(23, 0));
        
        setTestTime(2, 0); // 2 AM
        TEST_ASSERT_TRUE(plugin->isTimeInWindow(2, 0));
        
        setTestTime(5, 0); // 5 AM
        TEST_ASSERT_FALSE(plugin->isTimeInWindow(5, 0));
    }

    void testStatistics() {
        setTestTime(20, 0);
        
        // Generate some traffic
        for (int i = 0; i < 10; i++) {
            radio->send(createTestPacket(i));
        }
        
        auto stats = plugin->getStatistics();
        TEST_ASSERT_EQUAL(5, stats.queuedPackets);
        TEST_ASSERT_EQUAL(5, stats.droppedPackets);
        TEST_ASSERT_TRUE(stats.avgQueueTime > 0);
    }

    // Helper methods
    void setTestTime(int hour, int minute, int seconds = 0) {
        struct tm timeinfo = {};
        timeinfo.tm_hour = hour;
        timeinfo.tm_min = minute;
        timeinfo.tm_sec = seconds;
        time_t testTime = mktime(&timeinfo);
        // Set system time for testing
        setTime(testTime);
    }

    meshtastic_MeshPacket* createTestPacket(int id, bool highPriority = false) {
        meshtastic_MeshPacket* p = packetPool.allocZeroed();
        p->id = id;
        p->priority = highPriority ? 
            meshtastic_MeshPacket_Priority_RELIABLE : 
            meshtastic_MeshPacket_Priority_DEFAULT;
        return p;
    }
};

void setUp() {
    TestTimeWindow test;
    test.setUp();
}

void tearDown() {
    TestTimeWindow test;
    test.tearDown();
}

void runTests() {
    UNITY_BEGIN();
    
    TestTimeWindow test;
    test.runTests();
    
    UNITY_END();
}

#ifdef ARDUINO
void setup() {
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