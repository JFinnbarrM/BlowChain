#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/hci.h>
#include "new_sensor_lib.h"  // Your custom sensor library

#define ADV_UPDATE_INTERVAL_MS 1000

LOG_MODULE_REGISTER(disco_bt, LOG_LEVEL_INF);

struct sensor_data {
    int x;
    int y;
    int z;
    int xf;
    int yf;
    int zf;
};

static int bluetooth_ready = 0;

// Compact encoder: combine signed int and fraction into a byte pair
static inline int8_t compress_sample(int value, int fraction) {
    return (int8_t)(value);  // crude compression: just use whole number
}

static void start_advertising_disco(void)
{
    printk("Starting advertising...\n");

    struct bt_le_adv_param param = {
        .options = BT_LE_ADV_OPT_USE_IDENTITY,
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
        .id = 0,
    };

    struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
        // Initial dummy manufacturer data, will be updated
        BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA, 0x4C, 0x00, 0x01, 0x02, 0x03),
    };

    int err = bt_le_adv_start(&param, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Failed to start advertising (err %d)\n", err);
        return;
    }

    printk("Advertising started successfully\n");

    while (1) {
        struct sensor_data a = convert_and_collect(accel, SENSOR_CHAN_ACCEL_XYZ);
        struct sensor_data m = convert_and_collect(magneto, SENSOR_CHAN_MAGN_XYZ);

        // Compress sensor data into raw bytes
        uint8_t sensor_payload[] = {
            0x4C, 0x00, // Apple company ID (example)
            compress_sample(m.x, m.xf),
            compress_sample(m.y, m.yf),
            compress_sample(m.z, m.zf),
            compress_sample(a.x, a.xf),
            compress_sample(a.y, a.yf),
            compress_sample(a.z, a.zf),
        };

        struct bt_data updated_ad[] = {
            BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
            BT_DATA(BT_DATA_MANUFACTURER_DATA, sensor_payload, sizeof(sensor_payload)),
        };

        err = bt_le_adv_update_data(updated_ad, ARRAY_SIZE(updated_ad), NULL, 0);
        printk("Advertising update result: %d\n", err);

        printk("Accel: X=%d Y=%d Z=%d | Mag: X=%d Y=%d Z=%d\n",
               a.x, a.y, a.z, m.x, m.y, m.z);

        k_msleep(ADV_UPDATE_INTERVAL_MS);
    }
}

void sender_thread_disco(void)
{
    while (!bluetooth_ready) {
        k_msleep(100);
    }
    start_advertising_disco();
}

void bluetooth_ready_cb(int err)
{
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }
    printk("Bluetooth initialized\n");
    bluetooth_ready = 1;
}

void bluetooth_init_disco(void)
{
    int err = bt_enable(bluetooth_ready_cb);
    if (err) {
        printk("bt_enable failed (err %d)\n", err);
    }
}