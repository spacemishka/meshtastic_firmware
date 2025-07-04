#include "mesh/RadioInterface.h"
#include "plugins/TimeWindowPlugin.h"
#include "mesh/TimeWindow.h"
#include <benchmark/benchmark.h>
#include <vector>
#include <random>

class BenchmarkRadio : public RadioInterface {
public:
    std::vector<meshtastic_MeshPacket*> queue;
    ErrorCode send(meshtastic_MeshPacket* p) override { 
        queue.push_back(p);
        return ERRNO_OK;
    }
    void clear() { queue.clear(); }
};

// Benchmark fixture
class TimeWindowBenchmark : public benchmark::Fixture {
protected:
    BenchmarkRadio* radio;
    TimeWindowPlugin* plugin;
    std::mt19937 rng;
    std::vector<meshtastic_MeshPacket*> testPackets;

    void SetUp(const benchmark::State& state) {
        radio = new BenchmarkRadio();
        plugin = new TimeWindowPlugin();
        
        // Setup configuration
        config.has_lora = true;
        config.lora.time_window_enabled = true;
        config.lora.window_start_hour = 9;
        config.lora.window_start_minute = 0;
        config.lora.window_end_hour = 17;
        config.lora.window_end_minute = 0;
        config.lora.window_mode = meshtastic_TimeWindowMode_QUEUE_PACKETS;
        config.lora.window_queue_size = state.range(0); // Configurable queue size
        
        // Generate test packets
        generateTestPackets(state.range(1)); // Configurable packet count
    }

    void TearDown(const benchmark::State& state) {
        for (auto p : testPackets) {
            packetPool.release(p);
        }
        testPackets.clear();
        radio->clear();
        delete radio;
        delete plugin;
    }

    void generateTestPackets(size_t count) {
        std::uniform_int_distribution<> priorityDist(0, 2);
        std::uniform_int_distribution<> sizeDist(10, 200);
        
        for (size_t i = 0; i < count; i++) {
            auto p = packetPool.allocZeroed();
            p->id = i;
            p->priority = static_cast<meshtastic_MeshPacket_Priority>(priorityDist(rng));
            p->want_ack = (i % 3 == 0);
            p->hop_limit = 3;
            p->channel = 0;
            
            // Generate random payload
            size_t size = sizeDist(rng);
            p->payload.size = size;
            for (size_t j = 0; j < size; j++) {
                p->payload.bytes[j] = static_cast<uint8_t>(j & 0xFF);
            }
            
            testPackets.push_back(p);
        }
    }
};

// Benchmark packet queueing performance
BENCHMARK_DEFINE_F(TimeWindowBenchmark, Queueing)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        radio->clear();
        state.ResumeTiming();
        
        for (auto p : testPackets) {
            radio->send(p);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * testPackets.size());
    state.SetBytesProcessed(state.iterations() * std::accumulate(
        testPackets.begin(), testPackets.end(), 0ULL,
        [](size_t sum, const meshtastic_MeshPacket* p) {
            return sum + p->payload.size;
        }));
}

// Benchmark queue processing performance
BENCHMARK_DEFINE_F(TimeWindowBenchmark, Processing)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        // Fill queue
        for (auto p : testPackets) {
            radio->send(p);
        }
        state.ResumeTiming();
        
        // Process queue
        plugin->runOnce();
    }
    
    state.SetItemsProcessed(state.iterations() * testPackets.size());
}

// Benchmark priority sorting performance
BENCHMARK_DEFINE_F(TimeWindowBenchmark, PrioritySorting)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        std::vector<meshtastic_MeshPacket*> packets = testPackets;
        state.ResumeTiming();
        
        std::sort(packets.begin(), packets.end(), 
            [](const meshtastic_MeshPacket* a, const meshtastic_MeshPacket* b) {
                return a->priority > b->priority;
            });
    }
}

// Benchmark packet expiry checking
BENCHMARK_DEFINE_F(TimeWindowBenchmark, ExpiryCheck)(benchmark::State& state) {
    std::uniform_int_distribution<> timeDist(0, 7200); // 0-2 hours
    
    for (auto _ : state) {
        state.PauseTiming();
        // Set random enqueue times
        std::vector<uint32_t> enqueueTimes;
        for (size_t i = 0; i < testPackets.size(); i++) {
            enqueueTimes.push_back(timeDist(rng));
        }
        state.ResumeTiming();
        
        // Check expiry
        uint32_t now = 3600; // 1 hour mark
        size_t expired = 0;
        for (size_t i = 0; i < testPackets.size(); i++) {
            if (now - enqueueTimes[i] >= config.lora.window_packet_expire_secs) {
                expired++;
            }
        }
        benchmark::DoNotOptimize(expired);
    }
}

// Register benchmarks with different queue sizes and packet counts
BENCHMARK_REGISTER_F(TimeWindowBenchmark, Queueing)
    ->Args({32, 100})   // Small queue, few packets
    ->Args({128, 1000}) // Medium queue, many packets
    ->Args({512, 5000}) // Large queue, lots of packets
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(TimeWindowBenchmark, Processing)
    ->Args({32, 100})
    ->Args({128, 1000})
    ->Args({512, 5000})
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(TimeWindowBenchmark, PrioritySorting)
    ->Args({32, 100})
    ->Args({128, 1000})
    ->Args({512, 5000})
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(TimeWindowBenchmark, ExpiryCheck)
    ->Args({32, 100})
    ->Args({128, 1000})
    ->Args({512, 5000})
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();