#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/hci.h>
#include "new_sensor_lib.h"

#define IBEACON_RSSI 0xFF
#define NAME_LEN 30
  
static struct bt_data default_ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA,
              0x4c, 0x00, /* Apple */
              0x02, 0x15, /* iBeacon */
              0x00, 0x00, 0x00, 0x00, /* UUID[15..12] */
              0x00, 0x00, /* UUID  [11..10] */
              0x00, 0x00, /* UUID[9..8] */
              0x00, 0x00, /* UUID[7..6] */
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* UUID[5..0] */
              0x00, 0x00, /* Major - will be updated */
              0x00, 0x00, /* Minor - will be updated with distance */
              IBEACON_RSSI) /* Calibrated RSSI @ 1m */
};

struct sensor_data {
    int x;
    int y;
    int z;
	int xf;
    int yf;
    int zf;
};

static void start_advertising_disco(void) { 
    printk("shall we start?\n");   
    struct bt_le_adv_param param = {
        .options = BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_CONNECTABLE,
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
        .id = 0, 
    };
    
    bt_le_adv_start(&param, default_ad, ARRAY_SIZE(default_ad), NULL, 0);
    printk("yes we shall!\n");

    while (1) {
        struct sensor_data a = convert_and_collect(accel, SENSOR_CHAN_ACCEL_XYZ);
        struct sensor_data m = convert_and_collect(magneto, SENSOR_CHAN_MAGN_XYZ);

        a.z -= 10;

        struct bt_data dynamic_ad[] = {
            BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
            BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA,
            0x4c, 0x00, /* Apple */
            0x02, 0x15, /* iBeacon */
            0x00, 0x00, 0x00, 0x00, /* UUID[15..12] */
            0x00, 0x00, /* UUID  [11..10] */
            0x00, 0x00, /* UUID[9..8] */
            0x00, m.x, /* UUID[7..6] */
            m.xf, m.y, m.yf, m.z, m.zf, a.x, /* UUID[5..0] */
            a.xf, a.y, /* Major - will be updated */
            a.yf, a.z, /* Minor - will be updated with distance */
            a.zf) /* Calibrated RSSI @ 1m */
        };

        printk("Magnetometer: X: %d.%02dµT Y: %d.%02dµT Z: %d.%02dµT\n", 
        m.x, m.xf,
        m.y, m.yf,
        m.z, m.zf);

        printk("Accelerometer: X: %d.%02d ms/2 Y: %d.%02dms/2 Z: %d.%02dms/2\n", 
                a.x, a.xf,
                a.y, a.yf,
                a.z, a.zf);

        // bt_le_adv_update_data(dynamic_ad, ARRAY_SIZE(dynamic_ad), NULL, 0);
            // k_sleep(K_MSEC(2000));
        printk("Advertising update result: %d\n", bt_le_adv_update_data(dynamic_ad, ARRAY_SIZE(dynamic_ad), NULL, 0));
        k_msleep(1000);
    }
}

void sender_thread_disco(void) {
    printf("gonna try :)\n");
    k_sleep(K_MSEC(3000));
    start_advertising_disco();
    printf("adv started!!!!!\n");
}

void bluetooth_init_disco(void) {
    k_sleep(K_MSEC(3000));
  bt_addr_le_t mobile_addr;
  printf("1: %d\n", bt_addr_le_from_str("DA:BB:CC:BB:CC:FF", "random", &mobile_addr));
  printf("2: %d\n", bt_id_create(&mobile_addr, NULL));
  printf("3: %d\n", bt_enable(NULL));
  printf("hiya\n");
}