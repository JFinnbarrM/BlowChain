

#include "broadcaster.h"
#include "shell.h"

#include <stdlib.h>
#include <zephyr/logging/log.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

LOG_MODULE_REGISTER(broadcaster);

// Function to prepare advertising data.
static void prepare_ad_data(uint16_t pos_x,
                            uint16_t pos_y,
                            uint16_t velocity, 
                            uint16_t distance,
                            const struct rssi_measurement *rssi_data,
                            uint8_t rssi_count,
                            uint8_t *buf, 
                            size_t *buf_len)
{
    uint8_t *ptr = buf;

    // Add manufacturer ID (0xFFFF used)).
    *ptr++ = 0xFF;  // LSB
    *ptr++ = 0xFF;  // MSB

    // Add position (1 byte X, 1 byte Y).
    *ptr++ = pos_x;
    *ptr++ = pos_y;

    // Add velocity (2 bytes, little endian).
    *ptr++ = (uint8_t)(velocity & 0xFF);
    *ptr++ = (uint8_t)(velocity >> 8);

    // Add Distance (2 bytes, little endian).
    *ptr++ = (uint8_t)(distance & 0xFF);
    *ptr++ = (uint8_t)(distance >> 8);

    // Add RSSI count (1 byte).
    *ptr++ = rssi_count;

    // Add RSSI measurements (pairs of: char, int8_t)
    for (uint8_t i = 0; i < rssi_count; i++) {
        *ptr++ = rssi_data[i].name;
        *ptr++ = (uint8_t)rssi_data[i].rssi;
    }

    // Subtract the starting address from the end address to get the length.
    *buf_len = ptr - buf;
}

void broadcaster_tfun(void)
{
    int err; // For error checking.

    // Initial position (X, Y)
    uint8_t pos_x = 15;  // X coordinate
    uint8_t pos_y = 20;  // Y coordinate
    // initial velocity, and distance.
    uint16_t velocity = 6;   // cm/s
    uint16_t distance = 9;  // cm
    // Initial RSSI measurements
    struct rssi_measurement rssi_data[8] = {};
    uint8_t rssi_count = sizeof(rssi_data) / sizeof(rssi_data[0]);
    
    // Buffer for custom advertising data
    uint8_t custom_data[31];  // Maximum advertising data length
    size_t custom_data_len = 0;

    prepare_ad_data(pos_x, pos_y, velocity, distance, rssi_data, rssi_count, 
                    custom_data, &custom_data_len);

    // Create the advertising data
    const struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
        BT_DATA(BT_DATA_MANUFACTURER_DATA, custom_data, custom_data_len)
    };

    // Start advertising
    err = bt_le_adv_start(BT_LE_ADV_NCONN_IDENTITY, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Advertising failed to start (err %d)", err);
    } else {
        LOG_INF("Advertising started");
    }

    // Update advertising data periodically
    while (1) {
        k_sleep(K_MSEC(2000)); // Every 2 seconds.

        // Update position, velocity, and distance.

        // Get RSSI data.
        struct rssi_measurement rssi_data[8] = {};
        get_rssi_data(rssi_data);
        uint8_t rssi_count = count_nodes();

        prepare_ad_data(pos_x, pos_y, velocity, distance, rssi_data, rssi_count, 
            custom_data, &custom_data_len);
        
        err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), NULL, 0);
        if (err) {
            LOG_ERR("Failed to update advertising data (err %d)", err);
        } else {
            LOG_INF("Advertising data updated");
        }
    }
}

K_THREAD_DEFINE(            // Static thread declaration.
    broadcaster_tid,        // Thread ID.
    1024,                   // Memory (1KB).
    broadcaster_tfun,       // Entry point.
    NULL, NULL, NULL,       // No parameters.
    1,                      // Priority.
    0,                      // No special options.
    1000                    // Start after 1 second to allow bluetooth initialisation.
);   