#include "TimeWindowCLI.h"
#include "configuration.h"
#include "NodeDB.h"
#include <cstring>
#include <cstdio>

#define INVALID_TIME_ERR "Invalid time format. Use HH:MM (00:00-23:59)"
#define WINDOW_DISABLED_ERR "Time window is not enabled"

static void printTimeWindowStatus()
{
    if (!config.has_lora || !config.lora.time_window_enabled) {
        printf("Time window: Disabled\n");
        return;
    }

    const char* mode_str;
    switch (config.lora.window_mode) {
        case meshtastic_TimeWindowMode_DROP_PACKETS:
            mode_str = "Drop packets";
            break;
        case meshtastic_TimeWindowMode_QUEUE_PACKETS:
            mode_str = "Queue packets";
            break;
        case meshtastic_TimeWindowMode_RECEIVE_ONLY:
            mode_str = "Receive only";
            break;
        default:
            mode_str = "Unknown";
    }

    printf("Time window: Enabled\n");
    printf("Window: %02d:%02d - %02d:%02d\n",
           config.lora.window_start_hour,
           config.lora.window_start_minute,
           config.lora.window_end_hour,
           config.lora.window_end_minute);
    printf("Mode: %s\n", mode_str);
    if (config.lora.window_mode == meshtastic_TimeWindowMode_QUEUE_PACKETS) {
        printf("Queue size: %d packets\n", config.lora.window_queue_size);
        printf("Packet expiry: %d seconds\n", config.lora.window_packet_expire_secs);
    }
}

static bool parseTime(const char* timeStr, uint8_t* hour, uint8_t* minute)
{
    int h, m;
    if (sscanf(timeStr, "%d:%d", &h, &m) != 2) {
        return false;
    }
    
    if (h < 0 || h > 23 || m < 0 || m > 59) {
        return false;
    }
    
    *hour = h;
    *minute = m;
    return true;
}

void timeWindowCmd(int argc, char** argv)
{
    if (argc < 2) {
        printf("Usage:\n");
        printf("  window status         - Show current time window status\n");
        printf("  window enable         - Enable time window\n");
        printf("  window disable        - Disable time window\n");
        printf("  window set START END  - Set window time (HH:MM format)\n");
        printf("  window mode MODE      - Set mode (drop/queue/receive)\n");
        printf("  window queue SIZE     - Set queue size (1-100)\n");
        printf("  window expire SECS    - Set packet expiry time in seconds\n");
        return;
    }

    const char* cmd = argv[1];
    
    if (strcmp(cmd, "status") == 0) {
        printTimeWindowStatus();
        return;
    }
    
    if (strcmp(cmd, "enable") == 0) {
        config.has_lora = true;
        config.lora.time_window_enabled = true;
        nodeDB->saveConfig();
        printf("Time window enabled\n");
        return;
    }
    
    if (strcmp(cmd, "disable") == 0) {
        config.lora.time_window_enabled = false;
        nodeDB->saveConfig();
        printf("Time window disabled\n");
        return;
    }
    
    if (strcmp(cmd, "set") == 0) {
        if (argc != 4) {
            printf("Usage: window set START_TIME END_TIME\n");
            return;
        }
        
        uint8_t startHour, startMinute, endHour, endMinute;
        if (!parseTime(argv[2], &startHour, &startMinute)) {
            printf("%s\n", INVALID_TIME_ERR);
            return;
        }
        if (!parseTime(argv[3], &endHour, &endMinute)) {
            printf("%s\n", INVALID_TIME_ERR);
            return;
        }
        
        config.has_lora = true;
        config.lora.window_start_hour = startHour;
        config.lora.window_start_minute = startMinute;
        config.lora.window_end_hour = endHour;
        config.lora.window_end_minute = endMinute;
        nodeDB->saveConfig();
        printf("Time window set to %02d:%02d - %02d:%02d\n", 
               startHour, startMinute, endHour, endMinute);
        return;
    }
    
    if (strcmp(cmd, "mode") == 0) {
        if (argc != 3) {
            printf("Usage: window mode [drop|queue|receive]\n");
            return;
        }
        
        const char* mode = argv[2];
        if (strcmp(mode, "drop") == 0) {
            config.lora.window_mode = meshtastic_TimeWindowMode_DROP_PACKETS;
        } else if (strcmp(mode, "queue") == 0) {
            config.lora.window_mode = meshtastic_TimeWindowMode_QUEUE_PACKETS;
        } else if (strcmp(mode, "receive") == 0) {
            config.lora.window_mode = meshtastic_TimeWindowMode_RECEIVE_ONLY;
        } else {
            printf("Invalid mode. Use: drop, queue, or receive\n");
            return;
        }
        
        nodeDB->saveConfig();
        printf("Time window mode set to: %s\n", mode);
        return;
    }
    
    if (strcmp(cmd, "queue") == 0) {
        if (argc != 3) {
            printf("Usage: window queue SIZE (1-100)\n");
            return;
        }
        
        int size = atoi(argv[2]);
        if (size < 1 || size > 100) {
            printf("Queue size must be between 1 and 100\n");
            return;
        }
        
        config.lora.window_queue_size = size;
        nodeDB->saveConfig();
        printf("Queue size set to %d packets\n", size);
        return;
    }
    
    if (strcmp(cmd, "expire") == 0) {
        if (argc != 3) {
            printf("Usage: window expire SECONDS\n");
            return;
        }
        
        int secs = atoi(argv[2]);
        if (secs < 1) {
            printf("Expiry time must be positive\n");
            return;
        }
        
        config.lora.window_packet_expire_secs = secs;
        nodeDB->saveConfig();
        printf("Packet expiry time set to %d seconds\n", secs);
        return;
    }
    
    printf("Unknown command. Use 'window' without arguments to see usage.\n");
}