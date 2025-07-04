#include "AdminModule.h"
#include "configuration.h"
#include "NodeDB.h"
// ... (keep other existing includes)

// ... (keep existing code until the LoRa config case)

void AdminModule::handleSetConfig(const meshtastic_Config &c)
{
    bool requiresReboot = true;
    uint32_t changes = SEGMENT_CONFIG;

    switch (c.which_payload_variant) {
    // ... (keep other cases)

    case meshtastic_Config_lora_tag: {
        LOG_INFO("Set config: LoRa");
        config.has_lora = true;
        
        // Check if radio parameters require reboot
        bool radioParamsChanged = 
            config.lora.use_preset != c.payload_variant.lora.use_preset ||
            config.lora.region != c.payload_variant.lora.region ||
            config.lora.modem_preset != c.payload_variant.lora.modem_preset ||
            config.lora.bandwidth != c.payload_variant.lora.bandwidth ||
            config.lora.spread_factor != c.payload_variant.lora.spread_factor ||
            config.lora.coding_rate != c.payload_variant.lora.coding_rate ||
            config.lora.tx_power != c.payload_variant.lora.tx_power ||
            config.lora.frequency_offset != c.payload_variant.lora.frequency_offset ||
            config.lora.override_frequency != c.payload_variant.lora.override_frequency ||
            config.lora.channel_num != c.payload_variant.lora.channel_num ||
            config.lora.sx126x_rx_boosted_gain != c.payload_variant.lora.sx126x_rx_boosted_gain;

        if (!radioParamsChanged) {
            requiresReboot = false;
        }

        // Validate time window settings
        if (c.payload_variant.lora.time_window_enabled) {
            if (c.payload_variant.lora.window_start_hour >= 24 ||
                c.payload_variant.lora.window_start_minute >= 60 ||
                c.payload_variant.lora.window_end_hour >= 24 ||
                c.payload_variant.lora.window_end_minute >= 60) {
                LOG_ERROR("Invalid time window settings");
                myReply = allocErrorResponse(meshtastic_Routing_Error_INVALID_SETTINGS, NULL);
                return;
            }

            // Set defaults for queue settings if needed
            if (c.payload_variant.lora.window_mode == meshtastic_TimeWindowMode_QUEUE_PACKETS) {
                if (c.payload_variant.lora.window_queue_size == 0) {
                    ((meshtastic_Config &)c).payload_variant.lora.window_queue_size = 32;
                }
                if (c.payload_variant.lora.window_packet_expire_secs == 0) {
                    ((meshtastic_Config &)c).payload_variant.lora.window_packet_expire_secs = 3600;
                }
            }
        }

#ifdef RF95_FAN_EN
        // Turn PA off if disabled by config
        if (c.payload_variant.lora.pa_fan_disabled) {
            digitalWrite(RF95_FAN_EN, LOW ^ 0);
        } else {
            digitalWrite(RF95_FAN_EN, HIGH ^ 0);
        }
#endif
        // Save the new configuration
        config.lora = c.payload_variant.lora;

        // Handle first-time region setting
        if (isRegionUnset && config.lora.region > meshtastic_Config_LoRaConfig_RegionCode_UNSET) {
            // ... (keep existing region initialization code)
        }
        break;
    }

    // ... (keep other cases and rest of the file)
    }

    if (requiresReboot && !hasOpenEditTransaction) {
        disableBluetooth();
    }

    saveChanges(changes, requiresReboot);
}

// ... (keep rest of the file)
