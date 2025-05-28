#include "zephyr/bluetooth/gap.h"
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

static char* static_mac_str = "dc:78:58:8d:01:42";

// Custom UUIDs (replace with your own)
#define BT_UUID_POSITION_SERVICE \
    BT_UUID_DECLARE_128(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)
#define BT_UUID_X_POS \
    BT_UUID_DECLARE_128(0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)
#define BT_UUID_Y_POS \
    BT_UUID_DECLARE_128(0xBB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)

static uint8_t x_pos = 0;
static uint8_t y_pos = 0;

// Callback when X is written
static ssize_t write_x_pos(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    if (len != 1) return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    x_pos = *((uint8_t*)buf);
    printk("X set to: %d\n", x_pos);
    return len;
}

// Callback when Y is written
static ssize_t write_y_pos(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    if (len != 1) return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    y_pos = *((uint8_t*)buf);
    printk("Y set to: %d\n", y_pos);
    return len;
}

// Callback when X is read
static ssize_t read_x_pos(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &x_pos, sizeof(x_pos));
}

// Callback when Y is read
static ssize_t read_y_pos(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &y_pos, sizeof(y_pos));
}

// GATT Service Definition
BT_GATT_SERVICE_DEFINE(pos_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_POSITION_SERVICE),
    BT_GATT_CHARACTERISTIC(BT_UUID_X_POS, 
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
        read_x_pos, write_x_pos, &x_pos),
    BT_GATT_CHARACTERISTIC(BT_UUID_Y_POS,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
        read_y_pos, write_y_pos, &y_pos),
);

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_NAME_COMPLETE, "viewer"),
    BT_DATA(BT_DATA_UUID128_ALL, BT_UUID_POSITION_SERVICE, 16),
};

int main(void) {
    int err;

    // Create the MAC address from the string.
    bt_addr_le_t mobile_addr;
    bt_addr_le_from_str(static_mac_str, "random", &mobile_addr);

    // Set the identity address to the MAC address.
    err = bt_id_create(&mobile_addr, NULL);
    if (err < 0) {
        printk("Failed to set static address as identity (ERROR: %d)\n", err);
        return -1;
    } else {
        printk("Static address (%s) set as identity\n", static_mac_str);
    }

    // Start bluetooth.
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return -1;
    }
    printk("BLE XY Positioner ready\n");

    // Start advertising
    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed (err %d)\n", err);
        return -1;
    }

    printk("Advertising successfully started\n");
}