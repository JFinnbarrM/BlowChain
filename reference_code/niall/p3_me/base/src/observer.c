
#include "shell.h"
#include "zephyr/bluetooth/addr.h"

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <stdbool.h>

LOG_MODULE_REGISTER(observer);

// Atomic (threadsafe) variable to store the distance value (cm)
atomic_t sonic_atomic = ATOMIC_INIT(0); 

uint16_t get_sonic(void) {
    return (uint16_t)atomic_get(&sonic_atomic);
}

char major_to_char(uint16_t major) {
    switch (major) {
        case  2753: return 'A';
        case 32975: return 'B';
        case 26679: return 'C';
        case 41747: return 'D';
        case 30679: return 'E';
        case  6195: return 'F';
        case 30525: return 'G';
        case 57395: return 'H';
        case 60345: return 'I';
        case 12249: return 'J';
        case 36748: return 'K';
        case 27564: return 'L';
        case 49247: return 'M';
        default: return '?'; // unknown major
    }
}

static void print_hex_dump(struct net_buf_simple *ad) 
{
    // Hex dump string needs 3 chars per byte: "FF ")
    char hex_str[3 * ad->len + 1];
    char *ptr = hex_str;
    
    for (int i = 0; i < ad->len; i++) {
        ptr += sprintf(ptr, "%02X ", ad->data[i]);
    }
    LOG_INF("Hex dump: %s", hex_str);
}

//static const char* mac_str_sonic = "DC:78:58:8D:01:F2";
//static const char* mac_str_thing = "DA:FF:AA:FF:AA:FF";

// Faster device checking.
static const bt_addr_le_t mac_addr_sonic = {
    .type = BT_ADDR_LE_RANDOM,
    .a = {.val = {0xF2, 0x01, 0x8D, 0x58, 0x78, 0xDC}}  // Reversed from string
};
static const bt_addr_le_t mac_addr_thing = {
    .type = BT_ADDR_LE_RANDOM,
    .a = {.val = {0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xDA}}  // Reversed from string
};

static void device_found(
    const bt_addr_le_t *addr, 
    int8_t rssi, 
    uint8_t type, 
    struct net_buf_simple *ad
) {
    //print_hex_dump(ad);
    bool is_mac_sonic = (bt_addr_le_cmp(addr, &mac_addr_sonic) == 0);
    bool is_mac_thing = (bt_addr_le_cmp(addr, &mac_addr_thing) == 0);

    if (is_mac_sonic) {
        uint8_t *data = ad->data;
        //uint8_t data_len = ad->len;

        //uint16_t major = (data[25] << 8) | data[26];
        uint16_t minor = (data[27] << 8) | data[28];
        atomic_set(&sonic_atomic, minor);
    }

    else if (is_mac_thing) { 
        uint8_t *data = ad->data;
        //uint8_t data_len = ad->len;

        uint16_t major = (data[25] << 8) | data[26];
        //uint16_t minor = (data[27] << 8) | data[28];

        int8_t ib_rssi = (int8_t) data[29];

        if (update_node_rssi(major_to_char(major), ib_rssi) == 0) {
            //LOG_INF("%c = %i", major_to_char(major), ib_rssi);
        }
    }
}

void observer_tfun(void) 
{
    int err; // For error checking.

    // Use passive scanning with extended duration
    struct bt_le_scan_param scan_param = {
        .type = BT_LE_SCAN_TYPE_PASSIVE, // Passive scanning.
        .options = BT_LE_SCAN_OPT_NONE, // No options.
        .interval = 0x0060, // 60 ms scan interval.
        .window = 0x0060, // 60 ms scan window.
    };
	 	 
	err = bt_le_scan_start(&scan_param, device_found);
    if (err) {
        LOG_ERR("Start scanning failed (err %d)\n", err);
        return;
    }

    LOG_INF("Scanning started");
    return;
}

K_THREAD_DEFINE(            // Static thread declaration.
    observer_tid,           // Thread ID.
    1024,                   // Memory (1KB).
    observer_tfun,          // Entry point.
    NULL, NULL, NULL,       // No parameters.
    1,                      // Priority.
    0,                      // No special options.
    1000                    // Start after 1 second to allow bluetooth initialisation.
);   