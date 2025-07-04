# Time Window Feature

The Time Window feature allows you to control when your Meshtastic device transmits messages. This is useful for power saving, regulatory compliance, or limiting radio activity to specific hours.

## Configuration

### Via WebUI

1. Navigate to **Radio > Time Window** in the web interface
2. Enable/disable the time window feature
3. Set window start and end times
4. Choose operation mode
5. Configure queue settings (if using queue mode)

### Via CLI

```bash
# Enable/disable time window
window enable
window disable

# Set time window
window set 21:00 23:00

# Set operation mode
window mode [drop|queue|receive]

# Configure queue (if using queue mode)
window queue 32       # Set queue size
window expire 3600   # Set packet expiry time (seconds)

# View status and statistics
window status
```

### Configuration Options

1. **Window Times**
   - `start_hour` (0-23): Start hour
   - `start_minute` (0-59): Start minute
   - `end_hour` (0-23): End hour
   - `end_minute` (0-59): End minute

2. **Operation Modes**
   - `DROP_PACKETS`: Discard packets outside window
   - `QUEUE_PACKETS`: Store packets for later transmission
   - `RECEIVE_ONLY`: Only disable transmit outside window

3. **Queue Settings** (for QUEUE_PACKETS mode)
   - `queue_size`: Maximum number of queued packets (default: 32)
   - `packet_expiry`: Time before packets expire (default: 3600 seconds)

## Operation

### Inside Time Window
- Normal radio operation
- Queued packets are transmitted (if any)
- New packets sent immediately

### Outside Time Window
Behavior depends on operation mode:

1. **Drop Mode**
   - New packets are discarded
   - No transmission attempts

2. **Queue Mode**
   - Packets stored in queue
   - Transmitted when window opens
   - Expired packets removed
   - Queue size limited

3. **Receive Mode**
   - Continue receiving packets
   - No transmission attempts
   - No packet queueing

## Integration

### With Other Features

- Works with all message types
- Respects channel settings
- Compatible with encryption
- Works with router/client roles
- Integrates with power saving

### Status Reporting

- Current window state
- Queue status (if applicable)
- Next window transition time
- Statistics available via CLI/WebUI

## Building

Enable the feature in your build:

```ini
# platformio.ini
[env:your_board]
build_flags = 
    ${env.build_flags}
    -D HAS_TIME_WINDOW
```

Default settings in platformio.ini:
```ini
[time_window_options]
enabled = true
start_hour = 21
start_minute = 0
end_hour = 23
end_minute = 0
mode = QUEUE_PACKETS
queue_size = 32
packet_expiry = 3600
```

## Limitations

1. Requires accurate time
   - Uses device RTC
   - NTP sync recommended
   - Falls back to uptime if no RTC

2. Queue Mode
   - Limited by available memory
   - Packets expire after configured time
   - Queue overflow drops oldest packets

3. General
   - Configuration changes require reboot
   - No priority queue support
   - Window times in 24-hour format

## Use Cases

1. **Power Saving**
   - Limit transmissions to specific hours
   - Use with sleep mode
   - Queue important messages

2. **Regulatory Compliance**
   - Restrict operation to allowed hours
   - Ensure quiet periods
   - Log operation times

3. **Network Management**
   - Control peak traffic periods
   - Coordinate node activity
   - Schedule updates

## Troubleshooting

1. Check current status:
   ```bash
   window status
   ```

2. View statistics:
   ```bash
   window stats
   ```

3. Common issues:
   - Incorrect time settings
   - Queue overflow
   - Expired packets
   - Memory constraints

## API Reference

See protocol documentation for integration details:
- `time_window.proto`: Message definitions
- `LoRaConfig`: Configuration options
- `TimeWindowPlugin`: Implementation
