#include "command_time_window.h"
#include "TimeWindowCLI.h"
#include "configuration.h"
#include "../mesh/generated/meshtastic/lora_config.pb.h"
#include "CommandRegistry.h"

void registerTimeWindowCommands()
{
    // Primary command
    static Command windowCmd = {"window", "Time window control",
        "Usage:\n"
        "  window status       - Show current time window status\n"
        "  window enable      - Enable time window\n"
        "  window disable     - Disable time window\n"
        "  window set HH:MM HH:MM - Set window start and end times\n"
        "  window mode MODE   - Set mode (drop/queue/receive)\n"
        "  window queue SIZE  - Set queue size (1-100)\n"
        "  window expire SECS - Set packet expiry time in seconds",
        [](const char *arg_string) { 
            // Parse args and call timeWindowCmd
            int argc = 0;
            char *argv[8]; // Max 8 arguments
            
            // Split arg_string into argc/argv
            char *str = strdup(arg_string);
            char *token = strtok(str, " ");
            while (token && argc < 8) {
                argv[argc++] = token;
                token = strtok(NULL, " ");
            }
            
            timeWindowCmd(argc, argv);
            free(str);
            return true;
        }
    };

    // Subcommands for tab completion
    static Command windowStatus = {"status", "Show time window status"};
    static Command windowEnable = {"enable", "Enable time window"};
    static Command windowDisable = {"disable", "Disable time window"};
    static Command windowSet = {"set", "Set window times (HH:MM HH:MM)"};
    static Command windowMode = {"mode", "Set operation mode (drop/queue/receive)"};
    static Command windowQueue = {"queue", "Set queue size"};
    static Command windowExpire = {"expire", "Set packet expiry time"};

    CommandRegistry::registerCommand(&windowCmd);
    CommandRegistry::registerSubCommand("window", &windowStatus);
    CommandRegistry::registerSubCommand("window", &windowEnable);
    CommandRegistry::registerSubCommand("window", &windowDisable);
    CommandRegistry::registerSubCommand("window", &windowSet);
    CommandRegistry::registerSubCommand("window", &windowMode);
    CommandRegistry::registerSubCommand("window", &windowQueue);
    CommandRegistry::registerSubCommand("window", &windowExpire);
}