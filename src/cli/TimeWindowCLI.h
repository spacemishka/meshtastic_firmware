#pragma once

/**
 * CLI command handler for time window configuration.
 * Provides commands to control when the radio is allowed to transmit.
 * 
 * Commands:
 * window status       - Show current time window status
 * window enable       - Enable time window
 * window disable      - Disable time window
 * window set HH:MM HH:MM - Set window start and end times
 * window mode MODE    - Set operation mode (drop/queue/receive)
 * window queue SIZE   - Set queue size for queuing mode
 * window expire SECS  - Set packet expiry time in seconds
 * 
 * @param argc Number of command line arguments
 * @param argv Array of command line arguments
 */
void timeWindowCmd(int argc, char** argv);