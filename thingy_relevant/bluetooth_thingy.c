#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <string.h>
#include "new_sensor_lib.h"

LOG_MODULE_REGISTER(voc_sensor, LOG_LEVEL_INF);

#define LOCKBOX_SERVICE_UUID     BT_UUID_DECLARE_16(0x1234)
#define VOC_SENSOR_CHAR_UUID     BT_UUID_DECLARE_16(0xFF06)

// VOC sensor simulation parameters
#define VOC_BASE_LEVEL           200   // Base VOC level in PPB
#define VOC_PRESENCE_SPIKE       800   // VOC spike when person present
#define VOC_READING_INTERVAL_MS  2000  // Send reading every 2 seconds

static struct bt_conn *lockbox_conn = NULL;
static struct bt_gatt_write_params write_params;
static bool connected_to_lockbox = false;
static uint16_t voc_char_handle = 0;

// GATT write callback
static void voc_write_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params) {
    if (err) {
        LOG_ERR("VOC write failed (err %u)", err);
    } else {
        LOG_INF("VOC data sent successfully");
    }
}

// Send VOC reading to lockbox
static void send_voc_reading(void) {
    if (!connected_to_lockbox || voc_char_handle == 0) {
        LOG_WRN("Not connected to lockbox or characteristic not found");
        return;
    }
    
    uint16_t voc_value = convert_and_collect(gas, SENSOR_CHAN_VOC);
    LOG_INF("DATA: %d\n", voc_value);
    
    static uint8_t voc_data[2];
    voc_data[0] = voc_value & 0xFF;         // Low byte
    voc_data[1] = (voc_value >> 8) & 0xFF;  // High byte
    
    write_params.func = voc_write_cb;
    write_params.handle = voc_char_handle;
    write_params.offset = 0;
    write_params.data = voc_data;
    write_params.length = 2;
    
    int err = bt_gatt_write(lockbox_conn, &write_params);
    if (err) {
        LOG_ERR("Failed to send VOC reading (err %d)", err);
    } else {
        LOG_INF("Sending VOC reading: %u PPB", voc_value);
    }
}

// GATT discovery callback
static uint8_t discover_voc_char(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                struct bt_gatt_discover_params *params) {
    if (!attr) {
        LOG_INF("VOC characteristic discovery complete");
        params->func = NULL;
        return BT_GATT_ITER_STOP;
    }
    
    if (bt_uuid_cmp(params->uuid, VOC_SENSOR_CHAR_UUID) == 0) {
        voc_char_handle = attr->handle;
        LOG_INF("Found VOC characteristic handle: %u", voc_char_handle);
        return BT_GATT_ITER_STOP;
    }
    
    return BT_GATT_ITER_CONTINUE;
}

// GATT service discovery callback
static uint8_t discover_lockbox_service(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                       struct bt_gatt_discover_params *params) {
    static struct bt_gatt_discover_params char_params;
    
    if (!attr) {
        LOG_ERR("Lockbox service not found");
        params->func = NULL;
        return BT_GATT_ITER_STOP;
    }
    
    LOG_INF("Found lockbox service, discovering VOC characteristic...");
    
    // Discover VOC characteristic
    char_params.uuid = VOC_SENSOR_CHAR_UUID;
    char_params.func = discover_voc_char;
    char_params.start_handle = attr->handle + 1;
    char_params.end_handle = 0xFFFF;
    char_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
    
    int err = bt_gatt_discover(conn, &char_params);
    if (err) {
        LOG_ERR("Failed to discover characteristics (err %d)", err);
    }
    
    return BT_GATT_ITER_STOP;
}

// Start service discovery
static void start_discovery(struct bt_conn *conn) {
    static struct bt_gatt_discover_params service_params;
    
    LOG_INF("Starting service discovery...");
    
    service_params.uuid = LOCKBOX_SERVICE_UUID;
    service_params.func = discover_lockbox_service;
    service_params.start_handle = 0x0001;
    service_params.end_handle = 0xFFFF;
    service_params.type = BT_GATT_DISCOVER_PRIMARY;
    
    int err = bt_gatt_discover(conn, &service_params);
    if (err) {
        LOG_ERR("Failed to start service discovery (err %d)", err);
    }
}

// BLE connection callbacks
static void connected(struct bt_conn *conn, uint8_t err) {
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }
    
    LOG_INF("Connected to lockbox");
    lockbox_conn = bt_conn_ref(conn);
    connected_to_lockbox = true;
    
    // Start discovering services
    start_discovery(conn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    LOG_INF("Disconnected from lockbox (reason %u)", reason);
    
    if (lockbox_conn) {
        bt_conn_unref(lockbox_conn);
        lockbox_conn = NULL;
    }
    
    connected_to_lockbox = false;
    voc_char_handle = 0;
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

// Scan callback
static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                   struct net_buf_simple *ad) {
    char addr_str[BT_ADDR_LE_STR_LEN];
    
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    // LOG_INF("help me im trapped\n");
    // Look for "SecureLockbox" in advertising data
    while (ad->len > 1) {
        uint8_t len = net_buf_simple_pull_u8(ad);
        uint8_t ad_type = net_buf_simple_pull_u8(ad);
        
        if (ad_type == BT_DATA_NAME_COMPLETE) { // "SecureLockbox" = 13 chars + 1 for type
            if (memcmp(ad->data, "SecureLockbox", 13) == 0) {
                LOG_INF("Found SecureLockbox at %s, connecting...", addr_str);
                
                // Stop scanning and connect
                bt_le_scan_stop();
                
                int err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
                                          BT_LE_CONN_PARAM_DEFAULT, &lockbox_conn);
                if (err) {
                    LOG_ERR("Failed to connect (err %d)", err);
                }
                return;
            }
        }
        
        net_buf_simple_pull(ad, len - 1);
    }
}

void bluetooth_init_thread(void) {
    int err;
    k_sleep(K_MSEC(3000));
    LOG_INF("VOC Sensor starting...");
    
    // Initialize Bluetooth
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return err;
    }
    printk("here! %d\n", err);
    LOG_INF("Bluetooth initialized, scanning for lockbox...");
    
    // Start scanning for lockbox
    err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, scan_cb);
    if (err) {
        LOG_ERR("Scanning failed to start (err %d)", err);
        return err;
    }
    
    LOG_INF("Scanning for SecureLockbox...");
    
    // Keep running
    LOG_WRN("help!!!\n");
    while (1) {
        k_sleep(K_MSEC(1000));
        
        // Restart scanning if disconnected
        if (!connected_to_lockbox) {
            LOG_INF("Reconnecting to lockbox...");
            bt_le_scan_start(BT_LE_SCAN_PASSIVE, scan_cb);
        }
    }

}

// VOC sensor thread
void voc_sensor_thread(void) {
    LOG_INF("VOC sensor thread started");
    
    while (1) {
        if (connected_to_lockbox && voc_char_handle != 0) {
            send_voc_reading();
        }
        
        k_sleep(K_MSEC(VOC_READING_INTERVAL_MS));
    }
}