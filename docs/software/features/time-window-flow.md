# Time Window Operation Flow

```mermaid
sequenceDiagram
    participant App as Application
    participant TW as TimeWindowPlugin
    participant Radio as RadioInterface
    participant Queue as PacketQueue
    participant Clock as RTC

    Note over TW: Window Status Check
    loop Every Check Interval
        TW->>Clock: Get Current Time
        Clock-->>TW: Current Time
        TW->>TW: Check Window Status
        alt Window State Changed
            TW->>Radio: Update Window State
            TW->>App: Notify State Change
        end
    end

    Note over App: Send Packet Request
    App->>Radio: send(packet)
    Radio->>TW: Check Window Status
    
    alt Window Open
        Radio->>Radio: Process Queued Packets
        Radio->>Radio: Send New Packet
        Radio-->>App: Success
    else Window Closed
        alt Mode: Drop Packets
            Radio->>Radio: Drop Packet
            Radio-->>App: Error: Outside Window
        else Mode: Queue Packets
            Radio->>Queue: Add to Queue
            alt Queue Full
                Queue-->>Radio: Queue Full
                Radio->>Radio: Drop Packet
                Radio-->>App: Error: Queue Full
            else Queue Space Available
                Queue-->>Radio: Packet Queued
                Radio-->>App: Success: Queued
            end
        else Mode: Receive Only
            Radio->>Radio: Drop Packet
            Radio-->>App: Error: Tx Disabled
        end
    end

    Note over TW: Window Opens
    TW->>Radio: Window Open Event
    Radio->>Queue: Get Queued Packets
    loop For Each Queued Packet
        Queue-->>Radio: Next Packet
        Radio->>Radio: Check Expiry
        alt Packet Valid
            Radio->>Radio: Send Packet
        else Packet Expired
            Radio->>Radio: Drop Packet
        end
    end
```

## Operation States

1. **Initial State**
   - Plugin monitors time
   - Radio checks window status
   - Queue empty

2. **Window Open**
   - Normal operation
   - Process queued packets
   - Accept new packets

3. **Window Closed**
   - Drop/Queue/Receive-only
   - Monitor time
   - Maintain queue

4. **State Transition**
   - Process queue
   - Update status
   - Notify observers

## Data Flow

1. **Packet Processing**
   ```
   [Application] -> [Radio Interface]
                -> [Time Window Check]
                -> [Mode Selection]
                -> [Queue/Drop/Block]
   ```

2. **Queue Management**
   ```
   [Queue] -> [Expiry Check]
           -> [Size Management]
           -> [Priority Handling]
   ```

3. **Status Updates**
   ```
   [Time Check] -> [State Update]
                -> [Observer Notification]
                -> [UI Update]
   ```

## Critical Paths

1. **Time Accuracy**
   - RTC synchronization
   - Window transitions
   - Packet timestamps

2. **Queue Management**
   - Memory utilization
   - Packet expiry
   - Queue overflow

3. **State Changes**
   - Quick response
   - Queue processing
   - Status updates
