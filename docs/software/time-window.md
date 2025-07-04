# Time Window Feature

The Time Window feature allows you to control when your Meshtastic device transmits messages. This is useful for power saving, regulatory compliance, or limiting radio activity to specific hours.

## Configuration

Time windows can be configured in the following ways:
- Through the Web UI
- Via the CLI
- Through the device configuration protocol

### Settings

- `time_window_enabled`: Enable/disable the time window feature
- `window_start_hour`: Start hour (0-23)
- `window_start_minute`: Start minute (0-59)
- `window_end_hour`: End hour (0-23)
- `window_end_minute`: End minute (0-59)
- `window_mode`: How to handle packets outside the allowed window
  - `DROP_PACKETS`: Discard packets outside window
  - `QUEUE_PACKETS`: Store packets for later transmission
  - `RECEIVE_ONLY`: Only disable transmit outside window
- `window_queue_size`: Maximum number of packets to queue (for QUEUE_PACKETS mode)
- `window_packet_expire_secs`: How long to keep queued packets before discarding

### Operation Modes

1. DROP_PACKETS Mode
   - Packets received outside the time window are discarded
   - Suitable when message delivery is not critical
   - Lowest power consumption

2. QUEUE_PACKETS Mode
   - Messages are stored when outside the window
   - Transmitted when the next window opens
   - Messages expire after `window_packet_expire_secs`
   - Queue limited to `window_queue_size` messages

3. RECEIVE_ONLY Mode
   - Device continues to receive messages
   - Only transmissions are blocked outside window
   - Useful for monitoring without contributing to traffic

## Examples

### Night-time Only Operation
```
time_window_enabled: true
window_start_hour: 21    # 9 PM
window_start_minute: 0
window_end_hour: 6      # 6 AM
window_end_minute: 0
window_mode: QUEUE_PACKETS
```

### Work Hours Only
```
time_window_enabled: true
window_start_hour: 9     # 9 AM
window_start_minute: 0
window_end_hour: 17     # 5 PM
window_end_minute: 0
window_mode: DROP_PACKETS
```

## Power Considerations

The Time Window feature can significantly reduce power consumption by limiting radio transmissions to specific hours. When used with QUEUE_PACKETS mode, there is a small memory overhead for storing queued messages.

## Integration with Other Features

- Works with all message types (direct, broadcast, routing)
- Respects channel settings and encryption
- Compatible with router and client roles
- Can be used alongside other power-saving features

## Limitations

- Requires accurate device time
- Queue size limited by available memory
- Queued messages consume memory until transmitted or expired
- May impact message delivery times

## CLI Commands

```bash
# Enable time window
time-window enable

# Set window hours
time-window set --start 21:00 --end 06:00

# Set operation mode
time-window mode queue

# Show current status
time-window status
```

## API Reference

See the `LoRaConfig` message in the protocol buffers definition for integration details.