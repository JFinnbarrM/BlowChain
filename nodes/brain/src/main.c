#include "lock.h"
#include "zephyr/bluetooth/gap.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(blockchain, LOG_LEVEL_INF);

static char* static_mac_str =  "dc:80:00:00:08:35";

#define MAX_TRANSACTIONS_PER_BLOCK 3
#define MAX_USER_ID_LEN 16
#define MAX_DATA_LEN 32
#define BLOCKCHAIN_FILE "/lfs/chain.dat"
#define MAX_BLOCKS 20
#define HASH_SIZE 8
#define MAX_FAILED_ATTEMPTS 3
#define PASSCODE_LEN 6
#define MAX_BLE_CONNECTIONS 4

#define BT_UUID_LOCKBOX_SERVICE      BT_UUID_DECLARE_16(0x1234)
#define BT_UUID_USERNAME             BT_UUID_DECLARE_16(0xAA01)
#define BT_UUID_BLOCK_INFO           BT_UUID_DECLARE_16(0xBB02)
#define BT_UUID_LOCK_STATUS          BT_UUID_DECLARE_16(0xCC03)
#define BT_UUID_USER_STATUS          BT_UUID_DECLARE_16(0xDD04)
#define BT_UUID_PASSCODE             BT_UUID_DECLARE_16(0xEE05)
#define BT_UUID_VOC_SENSOR           BT_UUID_DECLARE_16(0xFF06)

typedef enum {
    TX_USER_ADDED = 1,
    TX_ACCESS_GRANTED,
    TX_ACCESS_DENIED,
    TX_PRESENCE_DETECTED,
    TX_PASSCODE_GENERATED,
    TX_PASSCODE_VERIFIED,
    TX_PASSCODE_FAILED,
    TX_TAMPER_DETECTED,
    TX_SYSTEM_LOCKED,
    TX_SYSTEM_STARTUP
} transaction_type_t;

typedef enum {
    STATE_READY = 0,
    STATE_PRESENCE_DETECTED,
    STATE_WAITING_PASSCODE,
    STATE_LOCKED,
    STATE_SHUTDOWN
} system_state_t;

typedef enum {
    CLIENT_TYPE_UNKNOWN = 0,
    CLIENT_TYPE_PC,
    CLIENT_TYPE_VOC_SENSOR
} client_type_t;

typedef struct {
    uint32_t timestamp;
    transaction_type_t type;
    char user_id[MAX_USER_ID_LEN];
    char data[MAX_DATA_LEN];
    uint32_t hash;
} simple_transaction_t;

typedef struct {
    uint32_t id;
    uint32_t timestamp;
    uint32_t prev_hash;
    uint32_t nonce;
    uint16_t tx_count;
    simple_transaction_t transactions[MAX_TRANSACTIONS_PER_BLOCK];
    uint32_t block_hash;
} simple_block_t;

typedef struct {
    uint32_t total_blocks;
    uint32_t latest_hash;
    simple_block_t blocks[MAX_BLOCKS];
} blockchain_data_t;

typedef struct {
    system_state_t state;
    char current_passcode[PASSCODE_LEN + 1];
    char current_user[MAX_USER_ID_LEN];
    uint32_t failed_attempts;
    uint32_t passcode_timestamp;
    bool tamper_detected;
    bool system_locked;
} lockbox_state_t;

typedef struct {
    struct bt_conn *conn;
    client_type_t type;
    char identifier[16];
    uint32_t last_activity;
    bool is_active;
    bool voc_notifications_enabled;
    bool status_notifications_enabled;
    bool lock_notifications_enabled;
    bool block_notifications_enabled;
} connection_info_t;

static blockchain_data_t g_blockchain;
static lockbox_state_t g_lockbox_state = {0};
static struct k_mutex blockchain_mutex;
static struct k_mutex lockbox_mutex;
static struct k_msgq pending_tx_queue;
static simple_transaction_t pending_tx_buffer[10];
static char current_username[MAX_USER_ID_LEN] = "UNKNOWN";

<<<<<<< HEAD
#define VOC_PRESENCE_THRESHOLD 250
=======
static connection_info_t connections[MAX_BLE_CONNECTIONS];
static struct k_mutex connections_mutex;

#define VOC_PRESENCE_THRESHOLD 500
>>>>>>> 2179525d9a6bc5e21907a6b95a84eec0f3b3968c
static uint16_t current_voc_value = 0;
static uint32_t last_voc_timestamp = 0;

static int create_genesis_block(void);
static int save_blockchain(void);
static int load_blockchain(void);
static void send_alert_to_dashboard(const char *message);
static int reset_blockchain(void);

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
static struct fs_mount_t blockchain_fs_mount = {
    .type = FS_LITTLEFS,
    .fs_data = &storage,
    .storage_dev = (void *)FIXED_PARTITION_ID(storage_partition),
    .mnt_point = "/lfs",
};

// Forward declarations for GATT handlers
static ssize_t write_voc_sensor(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               const void *buf, uint16_t len, uint16_t offset, uint8_t flags);
static void voc_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value);
static ssize_t write_username(struct bt_conn *conn, const struct bt_gatt_attr *attr, 
                             const void *buf, uint16_t len, uint16_t offset, uint8_t flags);
static ssize_t read_username(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                            void *buf, uint16_t len, uint16_t offset);
static ssize_t read_block_info(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                              void *buf, uint16_t len, uint16_t offset);
static void block_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value);
static ssize_t read_lock_status(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset);
static void lock_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value);
static ssize_t read_user_status(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset);
static void status_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value);
static ssize_t write_passcode(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             const void *buf, uint16_t len, uint16_t offset, uint8_t flags);
static ssize_t read_passcode(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                            void *buf, uint16_t len, uint16_t offset);

// GATT Service Definition - MOVED TO TOP to fix compilation errors
BT_GATT_SERVICE_DEFINE(lockbox_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_LOCKBOX_SERVICE),
    
    BT_GATT_CHARACTERISTIC(BT_UUID_USERNAME, 
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
        read_username, write_username, NULL),
        
    BT_GATT_CHARACTERISTIC(BT_UUID_BLOCK_INFO,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_READ,
        read_block_info, NULL, NULL),
    BT_GATT_CCC(block_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
        
    BT_GATT_CHARACTERISTIC(BT_UUID_LOCK_STATUS,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_READ,
        read_lock_status, NULL, NULL),
    BT_GATT_CCC(lock_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
        
    BT_GATT_CHARACTERISTIC(BT_UUID_USER_STATUS,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_READ,
        read_user_status, NULL, NULL),
    BT_GATT_CCC(status_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
        
    BT_GATT_CHARACTERISTIC(BT_UUID_PASSCODE,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
        read_passcode, write_passcode, NULL),
        
    BT_GATT_CHARACTERISTIC(BT_UUID_VOC_SENSOR,
        BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_WRITE,
        NULL, write_voc_sensor, NULL),
    BT_GATT_CCC(voc_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

// Connection management functions
static int find_free_connection_slot(void) {
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (!connections[i].is_active) {
            return i;
        }
    }
    return -1;
}

static int find_connection_by_handle(struct bt_conn *conn) {
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].is_active && connections[i].conn == conn) {
            return i;
        }
    }
    return -1;
}

static void update_connection_activity(struct bt_conn *conn) {
    k_mutex_lock(&connections_mutex, K_FOREVER);
    int slot = find_connection_by_handle(conn);
    if (slot >= 0) {
        connections[slot].last_activity = k_uptime_get_32();
    }
    k_mutex_unlock(&connections_mutex);
}

static client_type_t get_connection_type(struct bt_conn *conn) {
    k_mutex_lock(&connections_mutex, K_FOREVER);
    int slot = find_connection_by_handle(conn);
    client_type_t type = (slot >= 0) ? connections[slot].type : CLIENT_TYPE_UNKNOWN;
    k_mutex_unlock(&connections_mutex);
    return type;
}

static void set_connection_type(struct bt_conn *conn, client_type_t type, const char *identifier) {
    k_mutex_lock(&connections_mutex, K_FOREVER);
    int slot = find_connection_by_handle(conn);
    if (slot >= 0) {
        LOG_INF("=== SETTING CONNECTION TYPE ===");
        LOG_INF("Slot: %d", slot);
        LOG_INF("Old type: %d (%s)", connections[slot].type, connections[slot].identifier);
        LOG_INF("New type: %d (%s)", type, identifier);
        
        connections[slot].type = type;
        strncpy(connections[slot].identifier, identifier, sizeof(connections[slot].identifier) - 1);
        connections[slot].identifier[sizeof(connections[slot].identifier) - 1] = '\0';
        connections[slot].last_activity = k_uptime_get_32();
        
        LOG_INF("*** Connection %d identified as %s ***", slot, identifier);
    } else {
        LOG_ERR("Failed to find connection for type setting");
    }
    k_mutex_unlock(&connections_mutex);
}

// Notification functions
static void notify_voc_data(uint16_t voc_value) {
    struct {
        uint16_t voc_ppb;
        uint16_t threshold;
        uint32_t timestamp;
    } voc_notification = {
        .voc_ppb = voc_value,
        .threshold = VOC_PRESENCE_THRESHOLD,
        .timestamp = k_uptime_get_32()
    };
    
    k_mutex_lock(&connections_mutex, K_FOREVER);
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].is_active && 
            connections[i].type == CLIENT_TYPE_PC && 
            connections[i].voc_notifications_enabled) {
            
            bt_gatt_notify(connections[i].conn, &lockbox_svc.attrs[13], 
                          &voc_notification, sizeof(voc_notification));
            LOG_INF("VOC notification sent to PC (slot %d): %u PPB", i, voc_value);
        }
    }
    k_mutex_unlock(&connections_mutex);
}

static void notify_lock_status(bool is_open) {
    uint8_t status = is_open ? 1 : 0;
    
    k_mutex_lock(&connections_mutex, K_FOREVER);
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].is_active && 
            connections[i].type == CLIENT_TYPE_PC && 
            connections[i].lock_notifications_enabled) {
            
            bt_gatt_notify(connections[i].conn, &lockbox_svc.attrs[7], 
                          &status, sizeof(status));
            LOG_INF("Lock status notification sent to PC (slot %d): %s", i, is_open ? "OPEN" : "CLOSED");
        }
    }
    k_mutex_unlock(&connections_mutex);
}

static void notify_user_status(void) {
    struct {
        uint8_t state;
        uint8_t failed_attempts;
        uint8_t system_locked;
        uint8_t tamper_detected;
    } user_status;
    
    k_mutex_lock(&lockbox_mutex, K_FOREVER);
    user_status.state = (uint8_t)g_lockbox_state.state;
    user_status.failed_attempts = (uint8_t)g_lockbox_state.failed_attempts;
    user_status.system_locked = g_lockbox_state.system_locked ? 1 : 0;
    user_status.tamper_detected = g_lockbox_state.tamper_detected ? 1 : 0;
    k_mutex_unlock(&lockbox_mutex);
    
    k_mutex_lock(&connections_mutex, K_FOREVER);
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].is_active && 
            connections[i].type == CLIENT_TYPE_PC && 
            connections[i].status_notifications_enabled) {
            
            bt_gatt_notify(connections[i].conn, &lockbox_svc.attrs[10], 
                          &user_status, sizeof(user_status));
            LOG_INF("User status notification sent to PC (slot %d): state=%d", i, user_status.state);
        }
    }
    k_mutex_unlock(&connections_mutex);
}

static void notify_block_info(void) {
    struct {
        uint32_t total_blocks;
        uint32_t latest_hash;
        uint32_t latest_block_id;
    } block_info;
    
    k_mutex_lock(&blockchain_mutex, K_FOREVER);
    block_info.total_blocks = g_blockchain.total_blocks;
    block_info.latest_hash = g_blockchain.latest_hash;
    block_info.latest_block_id = (g_blockchain.total_blocks > 0) ? 
                                g_blockchain.blocks[g_blockchain.total_blocks - 1].id : 0;
    k_mutex_unlock(&blockchain_mutex);
    
    k_mutex_lock(&connections_mutex, K_FOREVER);
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].is_active && 
            connections[i].type == CLIENT_TYPE_PC && 
            connections[i].block_notifications_enabled) {
            
            bt_gatt_notify(connections[i].conn, &lockbox_svc.attrs[4], 
                          &block_info, sizeof(block_info));
            LOG_INF("Block info notification sent to PC (slot %d): %u blocks", i, block_info.total_blocks);
        }
    }
    k_mutex_unlock(&connections_mutex);
}

// Hash and blockchain functions (simplified for space)
static uint32_t simple_hash(const void *data, size_t len) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t hash = 0x811c9dc5;
    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 0x01000193;
    }
    return hash;
}

static uint32_t calculate_block_hash(simple_block_t *block) {
    size_t hash_len = sizeof(simple_block_t) - sizeof(block->block_hash);
    return simple_hash(block, hash_len);
}

static void mine_block(simple_block_t *block) {
    const uint32_t difficulty = 0x0000FFFF;
    LOG_INF("Mining block %u...", block->id);
    block->nonce = 0;
    do {
        block->nonce++;
        block->block_hash = calculate_block_hash(block);
    } while (block->block_hash > difficulty);
    LOG_INF("Block %u mined! Hash: 0x%08x", block->id, block->block_hash);
}

// Blockchain functions (simplified - keeping key functionality)
static int save_blockchain(void) {
    struct fs_file_t file;
    int ret;
    memset(&file, 0, sizeof(file));
    ret = fs_open(&file, BLOCKCHAIN_FILE, FS_O_CREATE | FS_O_WRITE);
    if (ret < 0) {
        fs_close(&file);
        return ret;
    }
    ret = fs_write(&file, &g_blockchain, sizeof(blockchain_data_t));
    fs_close(&file);
    return ret;
}

static int load_blockchain(void) {
    struct fs_file_t file;
    int ret;
    memset(&file, 0, sizeof(file));
    ret = fs_open(&file, BLOCKCHAIN_FILE, FS_O_READ);
    if (ret < 0) {
        fs_close(&file);
        return create_genesis_block();
    }
    ret = fs_read(&file, &g_blockchain, sizeof(blockchain_data_t));
    fs_close(&file);
    return ret >= 0 ? 0 : ret;
}

static int create_genesis_block(void) {
    memset(&g_blockchain, 0, sizeof(blockchain_data_t));
    simple_block_t *genesis = &g_blockchain.blocks[0];
    genesis->id = 1;
    genesis->timestamp = k_uptime_get_32();
    genesis->prev_hash = 0;
    genesis->tx_count = 1;
    
    simple_transaction_t *tx = &genesis->transactions[0];
    tx->timestamp = genesis->timestamp;
    tx->type = TX_SYSTEM_STARTUP;
    strcpy(tx->user_id, "SYSTEM");
    strcpy(tx->data, "Genesis Block Created");
    tx->hash = simple_hash(tx, sizeof(simple_transaction_t) - sizeof(tx->hash));
    
    mine_block(genesis);
    g_blockchain.total_blocks = 1;
    g_blockchain.latest_hash = genesis->block_hash;
    
    LOG_INF("Genesis block created");
    return save_blockchain();
}

static int add_transaction(transaction_type_t type, const char *user_id, const char *data) {
    simple_transaction_t tx;
    tx.timestamp = k_uptime_get_32();
    tx.type = type;
    strncpy(tx.user_id, user_id, MAX_USER_ID_LEN - 1);
    tx.user_id[MAX_USER_ID_LEN - 1] = '\0';
    strncpy(tx.data, data, MAX_DATA_LEN - 1);
    tx.data[MAX_DATA_LEN - 1] = '\0';
    tx.hash = simple_hash(&tx, sizeof(simple_transaction_t) - sizeof(tx.hash));
    
    int ret = k_msgq_put(&pending_tx_queue, &tx, K_NO_WAIT);
    if (ret < 0) {
        LOG_ERR("Transaction queue full");
        return ret;
    }
    LOG_INF("Transaction added: %s - %s", user_id, data);
    return 0;
}

static int create_new_block(simple_transaction_t *transactions, int tx_count) {
    if (g_blockchain.total_blocks >= MAX_BLOCKS) {
        LOG_ERR("Blockchain full!");
        return -ENOSPC;
    }
    
    simple_block_t *new_block = &g_blockchain.blocks[g_blockchain.total_blocks];
    memset(new_block, 0, sizeof(simple_block_t));
    
    new_block->id = g_blockchain.total_blocks + 1;
    new_block->timestamp = k_uptime_get_32();
    new_block->prev_hash = g_blockchain.latest_hash;
    new_block->tx_count = tx_count;
    
    for (int i = 0; i < tx_count; i++) {
        new_block->transactions[i] = transactions[i];
    }
    
    mine_block(new_block);
    g_blockchain.total_blocks++;
    g_blockchain.latest_hash = new_block->block_hash;
    
    LOG_INF("New block created: #%u with %d transactions", new_block->id, tx_count);
    
    // Notify PC clients of new block
    notify_block_info();
    
    return save_blockchain();
}

static void generate_passcode(const char *user_id) {
    k_mutex_lock(&lockbox_mutex, K_FOREVER);
    uint32_t seed = k_uptime_get_32();
    snprintf(g_lockbox_state.current_passcode, sizeof(g_lockbox_state.current_passcode), 
             "%06u", (seed % 1000000));
    strcpy(g_lockbox_state.current_user, user_id);
    strcpy(current_username, user_id);
    g_lockbox_state.passcode_timestamp = k_uptime_get_32();
    // Don't change state here - let it stay in WAITING_PASSCODE
    g_lockbox_state.failed_attempts = 0;
    k_mutex_unlock(&lockbox_mutex);
    
    char data[MAX_DATA_LEN];
    snprintf(data, sizeof(data), "Passcode: %s", g_lockbox_state.current_passcode);
    add_transaction(TX_PASSCODE_GENERATED, user_id, data);
    
    // Notify PC clients of status change
    notify_user_status();
    
    LOG_INF("Passcode generated for %s: %s", user_id, g_lockbox_state.current_passcode);
}

static void process_voc_reading(uint16_t voc_ppb) {
    current_voc_value = voc_ppb;
    last_voc_timestamp = k_uptime_get_32();
    
    LOG_INF("VOC reading: %u PPB", voc_ppb);
    
    // Notify PC clients immediately
    notify_voc_data(voc_ppb);
    
    if (voc_ppb > VOC_PRESENCE_THRESHOLD) {
        k_mutex_lock(&lockbox_mutex, K_FOREVER);
        
        // Only trigger if system is ready (not locked or already processing)
        if (g_lockbox_state.state == STATE_READY) {
            LOG_INF("VOC threshold exceeded (%u > %u) - triggering presence detection", 
                    voc_ppb, VOC_PRESENCE_THRESHOLD);
            
            g_lockbox_state.state = STATE_PRESENCE_DETECTED;
            k_mutex_unlock(&lockbox_mutex);
            
            // Record presence detection (user-independent)
            char data[MAX_DATA_LEN];
            snprintf(data, sizeof(data), "VOC presence: %u PPB", voc_ppb);
            const char* user_for_log = strlen(g_lockbox_state.current_user) > 0 ? 
                                      g_lockbox_state.current_user : "UNKNOWN";
            add_transaction(TX_PRESENCE_DETECTED, user_for_log, data);
            
            k_mutex_lock(&lockbox_mutex, K_FOREVER);
            g_lockbox_state.state = STATE_WAITING_PASSCODE;
            k_mutex_unlock(&lockbox_mutex);
            
            // Notify PC clients of state change
            notify_user_status();
            
            LOG_INF("System ready for passcode entry (VOC triggered - user independent)");
        } else {
            LOG_DBG("VOC threshold exceeded but system not ready (state: %d)", g_lockbox_state.state);
            k_mutex_unlock(&lockbox_mutex);
        }
    }
}

static bool verify_passcode(const char *user_id, const char *entered_code) {
    k_mutex_lock(&lockbox_mutex, K_FOREVER);
    
    if (g_lockbox_state.state != STATE_WAITING_PASSCODE) {
        k_mutex_unlock(&lockbox_mutex);
        return false;
    }
    
    bool success = (strcmp(entered_code, g_lockbox_state.current_passcode) == 0);
    
    if (success) {
        g_lockbox_state.state = STATE_READY;
        g_lockbox_state.failed_attempts = 0;
        k_mutex_unlock(&lockbox_mutex);
        
        add_transaction(TX_PASSCODE_VERIFIED, user_id, "Access granted");
        lock_open();
        notify_lock_status(true);
        notify_user_status();
        
        LOG_INF("Access granted for %s", user_id);
        return true;
    } else {
        g_lockbox_state.failed_attempts++;
        
        if (g_lockbox_state.failed_attempts >= MAX_FAILED_ATTEMPTS) {
            g_lockbox_state.state = STATE_LOCKED;
            g_lockbox_state.system_locked = true;
            k_mutex_unlock(&lockbox_mutex);
            
            add_transaction(TX_SYSTEM_LOCKED, user_id, "System locked");
            lock_close();
            notify_lock_status(false);
            notify_user_status();
        } else {
            g_lockbox_state.state = STATE_READY;
            k_mutex_unlock(&lockbox_mutex);
            notify_user_status();
        }
        return false;
    }
}

static int reset_blockchain(void) {
    k_mutex_lock(&blockchain_mutex, K_FOREVER);
    k_mutex_lock(&lockbox_mutex, K_FOREVER);
    
    LOG_WRN("Resetting blockchain and lockbox state...");
    
    memset(&g_blockchain, 0, sizeof(blockchain_data_t));
    
    memset(&g_lockbox_state, 0, sizeof(lockbox_state_t));
    g_lockbox_state.state = STATE_READY;
    
    while (k_msgq_get(&pending_tx_queue, &(simple_transaction_t){0}, K_NO_WAIT) == 0) {
    }
    
    k_mutex_unlock(&lockbox_mutex);
    k_mutex_unlock(&blockchain_mutex);
    
    int ret = fs_unlink(BLOCKCHAIN_FILE);
    if (ret < 0 && ret != -ENOENT) {
        LOG_ERR("Failed to delete blockchain file: %d", ret);
        return ret;
    }
    
    ret = create_genesis_block();
    if (ret < 0) {
        LOG_ERR("Failed to create new genesis block: %d", ret);
        return ret;
    }
    
    LOG_INF("Blockchain reset complete - new genesis block created");
    return 0;
}

static void send_alert_to_dashboard(const char *message) {
    LOG_ERR("DASHBOARD ALERT: %s", message);
}
static ssize_t write_voc_sensor(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    LOG_INF("=== VOC WRITE RECEIVED ===");
    LOG_INF("Length: %u (expected: 2)", len);
    LOG_INF("Offset: %u", offset);
    LOG_INF("Current connection type: %d", get_connection_type(conn));
    
    if (len != 2) {
        LOG_ERR("Invalid VOC data length: %u", len);
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    
    // Identify connection as VOC sensor if unknown
    if (get_connection_type(conn) == CLIENT_TYPE_UNKNOWN) {
        LOG_INF("*** IDENTIFYING CONNECTION AS VOC SENSOR ***");
        set_connection_type(conn, CLIENT_TYPE_VOC_SENSOR, "VOC_SENSOR");
        
        // Verify identification worked
        client_type_t verified_type = get_connection_type(conn);
        LOG_INF("Connection type after identification: %d", verified_type);
        
        if (verified_type == CLIENT_TYPE_VOC_SENSOR) {
            LOG_INF("*** VOC SENSOR IDENTIFICATION SUCCESSFUL ***");
        } else {
            LOG_ERR("*** VOC SENSOR IDENTIFICATION FAILED ***");
        }
    } else {
        LOG_INF("Connection already identified as type: %d", get_connection_type(conn));
    }
    
    uint16_t voc_value = *(uint16_t*)buf;
    LOG_INF("VOC value received: %u PPB", voc_value);
    
    update_connection_activity(conn);
    process_voc_reading(voc_value);
    
    return len;
}

static void voc_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    bool notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("VOC notifications %s", notify_enabled ? "enabled" : "disabled");
    
    // Update notification settings for the connection that changed this
    // Note: In a full implementation, you'd track which connection this came from
    k_mutex_lock(&connections_mutex, K_FOREVER);
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].is_active && connections[i].type == CLIENT_TYPE_PC) {
            connections[i].voc_notifications_enabled = notify_enabled;
        }
    }
    k_mutex_unlock(&connections_mutex);
}

static ssize_t write_username(struct bt_conn *conn, const struct bt_gatt_attr *attr, 
                             const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    if (len >= MAX_USER_ID_LEN) return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    
    if (get_connection_type(conn) == CLIENT_TYPE_UNKNOWN) {
        set_connection_type(conn, CLIENT_TYPE_PC, "PC_CLIENT");
    }
    
    memset(current_username, 0, MAX_USER_ID_LEN);
    memcpy(current_username, buf, len);
    current_username[len] = '\0';
    
    update_connection_activity(conn);
    
    k_mutex_lock(&lockbox_mutex, K_FOREVER);
    system_state_t current_state = g_lockbox_state.state;
    k_mutex_unlock(&lockbox_mutex);
    
    if (current_state == STATE_WAITING_PASSCODE) {
        // Presence already detected by VOC - generate passcode now
        generate_passcode(current_username);
        LOG_INF("BLE: Username set to: %s (passcode generated - presence detected)", current_username);
    } else {
        // No presence detected yet - just store username
        k_mutex_lock(&lockbox_mutex, K_FOREVER);
        strcpy(g_lockbox_state.current_user, current_username);
        k_mutex_unlock(&lockbox_mutex);
        LOG_INF("BLE: Username set to: %s (waiting for presence detection)", current_username);
    }
    
    return len;
}

static ssize_t read_username(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                            void *buf, uint16_t len, uint16_t offset) {
    if (get_connection_type(conn) == CLIENT_TYPE_UNKNOWN) {
        set_connection_type(conn, CLIENT_TYPE_PC, "PC_CLIENT");
    }
    update_connection_activity(conn);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, current_username, strlen(current_username));
}

static ssize_t read_block_info(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                              void *buf, uint16_t len, uint16_t offset) {
    struct {
        uint32_t total_blocks;
        uint32_t latest_hash;
        uint32_t latest_block_id;
    } block_info;
    
    k_mutex_lock(&blockchain_mutex, K_FOREVER);
    block_info.total_blocks = g_blockchain.total_blocks;
    block_info.latest_hash = g_blockchain.latest_hash;
    block_info.latest_block_id = (g_blockchain.total_blocks > 0) ? 
                                g_blockchain.blocks[g_blockchain.total_blocks - 1].id : 0;
    k_mutex_unlock(&blockchain_mutex);
    
    update_connection_activity(conn);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &block_info, sizeof(block_info));
}

static void block_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    bool notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Block notifications %s", notify_enabled ? "enabled" : "disabled");
    
    k_mutex_lock(&connections_mutex, K_FOREVER);
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].is_active && connections[i].type == CLIENT_TYPE_PC) {
            connections[i].block_notifications_enabled = notify_enabled;
        }
    }
    k_mutex_unlock(&connections_mutex);
}

static ssize_t read_lock_status(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset) {
    uint8_t status = lock_is_open() ? 1 : 0;
    update_connection_activity(conn);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &status, sizeof(status));
}

static void lock_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    bool notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Lock status notifications %s", notify_enabled ? "enabled" : "disabled");
    
    k_mutex_lock(&connections_mutex, K_FOREVER);
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].is_active && connections[i].type == CLIENT_TYPE_PC) {
            connections[i].lock_notifications_enabled = notify_enabled;
        }
    }
    k_mutex_unlock(&connections_mutex);
}

static ssize_t read_user_status(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset) {
    struct {
        uint8_t state;
        uint8_t failed_attempts;
        uint8_t system_locked;
        uint8_t tamper_detected;
    } user_status;
    
    k_mutex_lock(&lockbox_mutex, K_FOREVER);
    user_status.state = (uint8_t)g_lockbox_state.state;
    user_status.failed_attempts = (uint8_t)g_lockbox_state.failed_attempts;
    user_status.system_locked = g_lockbox_state.system_locked ? 1 : 0;
    user_status.tamper_detected = g_lockbox_state.tamper_detected ? 1 : 0;
    k_mutex_unlock(&lockbox_mutex);
    
    update_connection_activity(conn);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &user_status, sizeof(user_status));
}

static void status_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    bool notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Status notifications %s", notify_enabled ? "enabled" : "disabled");
    
    k_mutex_lock(&connections_mutex, K_FOREVER);
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].is_active && connections[i].type == CLIENT_TYPE_PC) {
            connections[i].status_notifications_enabled = notify_enabled;
        }
    }
    k_mutex_unlock(&connections_mutex);
}

static ssize_t write_passcode(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    if (len != 6) return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    
    char passcode_str[7];
    memcpy(passcode_str, buf, 6);
    passcode_str[6] = '\0';
    
    bool success = verify_passcode(current_username, passcode_str);
    update_connection_activity(conn);
    
    LOG_INF("BLE: Passcode entered: %s - %s", passcode_str, success ? "SUCCESS" : "FAILED");
    return len;
}

static ssize_t read_passcode(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                            void *buf, uint16_t len, uint16_t offset) {
    k_mutex_lock(&lockbox_mutex, K_FOREVER);
    char *passcode = g_lockbox_state.current_passcode;
    k_mutex_unlock(&lockbox_mutex);
    
    update_connection_activity(conn);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, passcode, strlen(passcode));
}

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_NAME_COMPLETE, "SecureLockbox"),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x34, 0x12),
};

// Background processing
static void blockchain_processor(void) {
    simple_transaction_t pending_txs[MAX_TRANSACTIONS_PER_BLOCK];
    int tx_count = 0;
    
    LOG_INF("Blockchain processor started");
    
    while (1) {
        simple_transaction_t tx;
        
        if (k_msgq_get(&pending_tx_queue, &tx, K_MSEC(10000)) == 0) {
            k_mutex_lock(&blockchain_mutex, K_FOREVER);
            pending_txs[tx_count++] = tx;
            k_mutex_unlock(&blockchain_mutex);
            
            if (tx_count >= MAX_TRANSACTIONS_PER_BLOCK) {
                k_mutex_lock(&blockchain_mutex, K_FOREVER);
                create_new_block(pending_txs, tx_count);
                tx_count = 0;
                k_mutex_unlock(&blockchain_mutex);
            }
        } else if (tx_count > 0) {
            k_mutex_lock(&blockchain_mutex, K_FOREVER);
            create_new_block(pending_txs, tx_count);
            tx_count = 0;
            k_mutex_unlock(&blockchain_mutex);
        }
        
        k_sleep(K_MSEC(100));
    }
}

K_THREAD_DEFINE(blockchain_thread, 2048, blockchain_processor, 
                NULL, NULL, NULL, K_PRIO_COOP(7), 0, 0);

static void connected(struct bt_conn *conn, uint8_t err) {
    if (err) {
        LOG_ERR("BLE Connection failed (err 0x%02x)", err);
        return;
    }
    
    k_mutex_lock(&connections_mutex, K_FOREVER);
    int slot = find_free_connection_slot();
    if (slot >= 0) {
        connections[slot].conn = bt_conn_ref(conn);
        connections[slot].type = CLIENT_TYPE_UNKNOWN;
        connections[slot].is_active = true;
        connections[slot].last_activity = k_uptime_get_32();
        strcpy(connections[slot].identifier, "UNKNOWN");
        
        LOG_INF("=== BLE CLIENT CONNECTED ===");
        LOG_INF("Connection slot: %d", slot);
        LOG_INF("Initial type: CLIENT_TYPE_UNKNOWN (%d)", CLIENT_TYPE_UNKNOWN);
        LOG_INF("Waiting for client identification...");
    } else {
        LOG_ERR("No free connection slots available!");
        bt_conn_disconnect(conn, BT_HCI_ERR_INSUFFICIENT_RESOURCES);
    }
    k_mutex_unlock(&connections_mutex);
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    k_mutex_lock(&connections_mutex, K_FOREVER);
    int slot = find_connection_by_handle(conn);
    if (slot >= 0) {
        const char* client_name = connections[slot].identifier;
        LOG_INF("BLE %s Disconnected (slot %d, reason 0x%02x)", client_name, slot, reason);
        
        bt_conn_unref(connections[slot].conn);
        memset(&connections[slot], 0, sizeof(connection_info_t));
    }
    k_mutex_unlock(&connections_mutex);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

// Shell commands (enhanced with notifications)
static int cmd_status(const struct shell *sh, size_t argc, char **argv) {
    k_mutex_lock(&lockbox_mutex, K_FOREVER);
    k_mutex_lock(&blockchain_mutex, K_FOREVER);
    k_mutex_lock(&connections_mutex, K_FOREVER);
    
    shell_print(sh, "=== Lockbox Status ===");
    shell_print(sh, "State: %s", 
        g_lockbox_state.state == STATE_READY ? "READY" :
        g_lockbox_state.state == STATE_PRESENCE_DETECTED ? "PRESENCE_DETECTED" :
        g_lockbox_state.state == STATE_WAITING_PASSCODE ? "WAITING_PASSCODE" :
        g_lockbox_state.state == STATE_LOCKED ? "LOCKED" : "SHUTDOWN");
    
    shell_print(sh, "Current User: %s", g_lockbox_state.current_user);
    shell_print(sh, "Failed Attempts: %u/3", g_lockbox_state.failed_attempts);
    shell_print(sh, "System Locked: %s", g_lockbox_state.system_locked ? "YES" : "NO");
    shell_print(sh, "Tamper Detected: %s", g_lockbox_state.tamper_detected ? "YES" : "NO");
    shell_print(sh, "Lock Status: %s", lock_is_open() ? "OPEN" : "CLOSED");
    
    shell_print(sh, "\n=== VOC Sensor Status ===");
    shell_print(sh, "Current VOC: %u PPB", current_voc_value);
    shell_print(sh, "Threshold: %u PPB", VOC_PRESENCE_THRESHOLD);
    uint32_t time_since_reading = last_voc_timestamp > 0 ? k_uptime_get_32() - last_voc_timestamp : 0;
    shell_print(sh, "Last Reading: %u ms ago", time_since_reading);
    
    shell_print(sh, "\n=== BLE Connections ===");
    int active = 0, pc_clients = 0, voc_sensors = 0;
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].is_active) {
            active++;
            if (connections[i].type == CLIENT_TYPE_PC) pc_clients++;
            if (connections[i].type == CLIENT_TYPE_VOC_SENSOR) voc_sensors++;
            
            shell_print(sh, "  Slot %d: %s (%s) - %u ms ago", i, 
                       connections[i].identifier,
                       connections[i].type == CLIENT_TYPE_PC ? "PC" : "VOC",
                       k_uptime_get_32() - connections[i].last_activity);
        }
    }
    shell_print(sh, "Active: %d/%d (PC: %d, VOC: %d)", active, MAX_BLE_CONNECTIONS, pc_clients, voc_sensors);
    
    shell_print(sh, "\n=== Blockchain Status ===");
    shell_print(sh, "Total Blocks: %u", g_blockchain.total_blocks);
    shell_print(sh, "Latest Hash: 0x%08x", g_blockchain.latest_hash);
    
    k_mutex_unlock(&connections_mutex);
    k_mutex_unlock(&blockchain_mutex);
    k_mutex_unlock(&lockbox_mutex);
    
    return 0;
}

static int cmd_generate_passcode(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 2) {
        shell_error(sh, "Usage: generate_passcode <user_id>");
        return -EINVAL;
    }
    
    // Set the username and generate passcode (simulates presence detection)
    strcpy(current_username, argv[1]);
    
    k_mutex_lock(&lockbox_mutex, K_FOREVER);
    strcpy(g_lockbox_state.current_user, argv[1]);
    g_lockbox_state.state = STATE_WAITING_PASSCODE;
    k_mutex_unlock(&lockbox_mutex);
    
    generate_passcode(argv[1]);  // This calls notify_user_status()
    shell_print(sh, "Passcode generated for user: %s", argv[1]);
    shell_print(sh, "System state set to WAITING_PASSCODE");
    shell_print(sh, "BLE clients notified of status change");
    return 0;
}

static int cmd_detect_presence(const struct shell *sh, size_t argc, char **argv) {
    k_mutex_lock(&lockbox_mutex, K_FOREVER);
    
    if (g_lockbox_state.state == STATE_READY) {
        g_lockbox_state.state = STATE_PRESENCE_DETECTED;
        LOG_INF("Manual presence detected");
        
        k_mutex_unlock(&lockbox_mutex);
        
        const char* user_for_log = strlen(g_lockbox_state.current_user) > 0 ? 
                                  g_lockbox_state.current_user : "UNKNOWN";
        add_transaction(TX_PRESENCE_DETECTED, user_for_log, "Manual presence detection");
        
        k_mutex_lock(&lockbox_mutex, K_FOREVER);
        g_lockbox_state.state = STATE_WAITING_PASSCODE;
        k_mutex_unlock(&lockbox_mutex);
        
        // Notify BLE clients
        notify_user_status();
        
        shell_print(sh, "Presence detected (user-independent)");
        shell_print(sh, "System ready for username and passcode entry");
        shell_print(sh, "BLE clients notified of state change");
        LOG_INF("System ready for passcode entry (manual trigger)");
    } else {
        k_mutex_unlock(&lockbox_mutex);
        shell_error(sh, "System not ready for presence detection (current state: %d)", g_lockbox_state.state);
    }
    
    return 0;
}

static int cmd_enter_passcode(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 3) {
        shell_error(sh, "Usage: enter_passcode <user_id> <passcode>");
        return -EINVAL;
    }
    
    bool success = verify_passcode(argv[1], argv[2]);  // This calls notify_lock_status() and notify_user_status()
    shell_print(sh, "Passcode verification: %s", success ? "SUCCESS" : "FAILED");
    shell_print(sh, "BLE clients notified of status change");
    
    return 0;
}

static int cmd_trigger_tamper(const struct shell *sh, size_t argc, char **argv) {
    k_mutex_lock(&lockbox_mutex, K_FOREVER);
    g_lockbox_state.tamper_detected = true;
    g_lockbox_state.system_locked = true;
    g_lockbox_state.state = STATE_LOCKED;
    k_mutex_unlock(&lockbox_mutex);
    
    add_transaction(TX_TAMPER_DETECTED, "SYSTEM", "Manual tamper alert");
    send_alert_to_dashboard("CRITICAL: Manual tamper detected - Emergency lockdown");
    
    lock_close();
    
    // Notify BLE clients of all changes
    notify_user_status();
    notify_lock_status(false);
    
    shell_print(sh, "Tamper alert triggered - system locked");
    shell_print(sh, "BLE clients notified of lock and status changes");
    LOG_ERR("MANUAL TAMPER DETECTED - EMERGENCY LOCKDOWN");
    
    return 0;
}

static int cmd_open_lock(const struct shell *sh, size_t argc, char **argv) {
    lock_open();
    
    // Notify BLE clients
    notify_lock_status(true);
    
    shell_print(sh, "Lock manually opened");
    shell_print(sh, "BLE clients notified of lock status change");
    
    add_transaction(TX_ACCESS_GRANTED, "SHELL", "Manual lock open");
    
    return 0;
}

static int cmd_close_lock(const struct shell *sh, size_t argc, char **argv) {
    lock_close();
    
    // Notify BLE clients
    notify_lock_status(false);
    
    shell_print(sh, "Lock manually closed");
    shell_print(sh, "BLE clients notified of lock status change");
    
    add_transaction(TX_ACCESS_DENIED, "SHELL", "Manual lock close");
    
    return 0;
}

static int cmd_simulate_voc(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 2) {
        shell_error(sh, "Usage: simulate_voc <voc_value_ppb>");
        return -EINVAL;
    }
    
    uint16_t voc_value = (uint16_t)atoi(argv[1]);
    
    if (voc_value > 2000) {
        shell_error(sh, "VOC value too high (max 2000 PPB)");
        return -EINVAL;
    }
    
    process_voc_reading(voc_value);  // This calls notify_voc_data() and potentially notify_user_status()
    
    shell_print(sh, "Simulated VOC reading: %u PPB", voc_value);
    shell_print(sh, "BLE clients notified of VOC data");
    
    if (voc_value > VOC_PRESENCE_THRESHOLD) {
        shell_print(sh, "VOC above threshold - presence detection triggered");
        shell_print(sh, "BLE clients notified of status change");
    }
    
    return 0;
}

static int cmd_blockchain_stats(const struct shell *sh, size_t argc, char **argv) {
    k_mutex_lock(&blockchain_mutex, K_FOREVER);
    
    shell_print(sh, "=== Blockchain Statistics ===");
    shell_print(sh, "Total Blocks: %u", g_blockchain.total_blocks);
    shell_print(sh, "Latest Hash: 0x%08x", g_blockchain.latest_hash);
    
    int user_adds = 0, access_granted = 0, access_denied = 0, presence = 0;
    int passcode_gen = 0, passcode_ok = 0, passcode_fail = 0, tamper = 0, locked = 0;
    
    for (uint32_t i = 0; i < g_blockchain.total_blocks; i++) {
        simple_block_t *block = &g_blockchain.blocks[i];
        for (int j = 0; j < block->tx_count; j++) {
            switch (block->transactions[j].type) {
                case TX_USER_ADDED: user_adds++; break;
                case TX_ACCESS_GRANTED: access_granted++; break;
                case TX_ACCESS_DENIED: access_denied++; break;
                case TX_PRESENCE_DETECTED: presence++; break;
                case TX_PASSCODE_GENERATED: passcode_gen++; break;
                case TX_PASSCODE_VERIFIED: passcode_ok++; break;
                case TX_PASSCODE_FAILED: passcode_fail++; break;
                case TX_TAMPER_DETECTED: tamper++; break;
                case TX_SYSTEM_LOCKED: locked++; break;
                default: break;
            }
        }
    }
    
    shell_print(sh, "Passcodes Generated: %d", passcode_gen);
    shell_print(sh, "Passcodes Verified: %d", passcode_ok);
    shell_print(sh, "Failed Attempts: %d", passcode_fail);
    shell_print(sh, "Presence Events: %d", presence);
    shell_print(sh, "Tamper Events: %d", tamper);
    shell_print(sh, "System Lockouts: %d", locked);
    
    k_mutex_unlock(&blockchain_mutex);
    return 0;
}

static int cmd_reset(const struct shell *sh, size_t argc, char **argv) {
    shell_print(sh, "WARNING: This will permanently delete the blockchain!");
    
    if (argc > 1 && strcmp(argv[1], "YES") == 0) {
        int ret = reset_blockchain();
        if (ret == 0) {
            // Notify BLE clients of blockchain reset
            notify_block_info();
            notify_user_status();
            
            shell_print(sh, "Blockchain reset successfully");
            shell_print(sh, "BLE clients notified of blockchain reset");
        } else {
            shell_error(sh, "Failed to reset blockchain: %d", ret);
        }
        return ret;
    } else {
        shell_print(sh, "Reset cancelled. Use: lockbox reset YES");
        return 0;
    }
}

static int cmd_history(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 2) {
        shell_error(sh, "Usage: history <user_id>");
        return -EINVAL;
    }
    
    k_mutex_lock(&blockchain_mutex, K_FOREVER);
    
    shell_print(sh, "=== History for %s ===", argv[1]);
    
    for (uint32_t i = 0; i < g_blockchain.total_blocks; i++) {
        simple_block_t *block = &g_blockchain.blocks[i];
        for (int j = 0; j < block->tx_count; j++) {
            simple_transaction_t *tx = &block->transactions[j];
            if (strcmp(tx->user_id, argv[1]) == 0) {
                shell_print(sh, "Block %u: Time %u, Type %d, Data: %s", 
                           block->id, tx->timestamp, tx->type, tx->data);
            }
        }
    }
    
    k_mutex_unlock(&blockchain_mutex);
    return 0;
}

static int cmd_connections(const struct shell *sh, size_t argc, char **argv) {
    k_mutex_lock(&connections_mutex, K_FOREVER);
    
    shell_print(sh, "=== BLE Connection Details ===");
    shell_print(sh, "Max Connections: %d", MAX_BLE_CONNECTIONS);
    
    int active_count = 0;
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].is_active) {
            active_count++;
            uint32_t inactive_time = k_uptime_get_32() - connections[i].last_activity;
            
            shell_print(sh, "\nSlot %d: ACTIVE", i);
            shell_print(sh, "  Type: %s", connections[i].identifier);
            shell_print(sh, "  Client: %s", 
                       connections[i].type == CLIENT_TYPE_PC ? "PC" : 
                       connections[i].type == CLIENT_TYPE_VOC_SENSOR ? "VOC Sensor" : "Unknown");
            shell_print(sh, "  Last Activity: %u ms ago", inactive_time);
            shell_print(sh, "  Notifications: VOC=%s, Lock=%s, Status=%s, Block=%s",
                       connections[i].voc_notifications_enabled ? "ON" : "OFF",
                       connections[i].lock_notifications_enabled ? "ON" : "OFF",
                       connections[i].status_notifications_enabled ? "ON" : "OFF",
                       connections[i].block_notifications_enabled ? "ON" : "OFF");
        } else {
            shell_print(sh, "Slot %d: FREE", i);
        }
    }
    
    shell_print(sh, "\nSummary: %d/%d slots used", active_count, MAX_BLE_CONNECTIONS);
    
    k_mutex_unlock(&connections_mutex);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(lockbox_cmds,
    SHELL_CMD(status, NULL, "Show lockbox and blockchain status", cmd_status),
    SHELL_CMD(connections, NULL, "Show BLE connection details", cmd_connections),
    SHELL_CMD(generate_passcode, NULL, "Generate passcode for user", cmd_generate_passcode),
    SHELL_CMD(detect_presence, NULL, "Simulate presence detection (user-independent)", cmd_detect_presence),
    SHELL_CMD(enter_passcode, NULL, "Enter passcode for verification", cmd_enter_passcode),
    SHELL_CMD(trigger_tamper, NULL, "Trigger tamper alert", cmd_trigger_tamper),
    SHELL_CMD(open_lock, NULL, "Manually open lock", cmd_open_lock),
    SHELL_CMD(close_lock, NULL, "Manually close lock", cmd_close_lock),
    SHELL_CMD(simulate_voc, NULL, "Simulate VOC reading <ppb>", cmd_simulate_voc),
    SHELL_CMD(blockchain_stats, NULL, "Show blockchain statistics", cmd_blockchain_stats),
    SHELL_CMD(history, NULL, "Show user transaction history", cmd_history),
    SHELL_CMD(reset, NULL, "Reset blockchain (WARNING: Destructive!)", cmd_reset),
    SHELL_SUBCMD_SET_END
);

<<<<<<< HEAD
SHELL_CMD_REGISTER(lockbox, &lockbox_cmds, "Lockbox blockchain commands", NULL);

static ssize_t write_username(struct bt_conn *conn, const struct bt_gatt_attr *attr, 
                             const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    if (len >= MAX_USER_ID_LEN) return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    
    memset(current_username, 0, MAX_USER_ID_LEN);
    memcpy(current_username, buf, len);
    current_username[len] = '\0';
    
    generate_passcode(current_username);
    
    LOG_INF("BLE: Username set to: %s", current_username);
    return len;
}

static ssize_t read_username(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                            void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, current_username, strlen(current_username));
}

static ssize_t read_block_info(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                              void *buf, uint16_t len, uint16_t offset) {
    struct {
        uint32_t total_blocks;
        uint32_t latest_hash;
        uint32_t latest_block_id;
    } block_info;
    
    k_mutex_lock(&blockchain_mutex, K_FOREVER);
    block_info.total_blocks = g_blockchain.total_blocks;
    block_info.latest_hash = g_blockchain.latest_hash;
    block_info.latest_block_id = (g_blockchain.total_blocks > 0) ? 
                                g_blockchain.blocks[g_blockchain.total_blocks - 1].id : 0;
    k_mutex_unlock(&blockchain_mutex);
    
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &block_info, sizeof(block_info));
}

static ssize_t read_lock_status(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset) {
    uint8_t status = lock_is_open() ? 1 : 0;
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &status, sizeof(status));
}

static ssize_t read_user_status(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset) {
    struct {
        uint8_t state;
        uint8_t failed_attempts;
        uint8_t system_locked;
        uint8_t tamper_detected;
    } user_status;
    
    k_mutex_lock(&lockbox_mutex, K_FOREVER);
    user_status.state = (uint8_t)g_lockbox_state.state;
    user_status.failed_attempts = (uint8_t)g_lockbox_state.failed_attempts;
    user_status.system_locked = g_lockbox_state.system_locked ? 1 : 0;
    user_status.tamper_detected = g_lockbox_state.tamper_detected ? 1 : 0;
    k_mutex_unlock(&lockbox_mutex);
    
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &user_status, sizeof(user_status));
}

static ssize_t write_passcode(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    if (len != 6) return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    
    char passcode_str[7];
    memcpy(passcode_str, buf, 6);
    passcode_str[6] = '\0';
    
    bool success = verify_passcode(current_username, passcode_str);
    if (success) {
        lock_open();
    } else {
        lock_close();
    }
    
    LOG_INF("BLE: Passcode entered: %s - %s", passcode_str, success ? "SUCCESS" : "FAILED");
    return len;
}

static ssize_t read_passcode(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                            void *buf, uint16_t len, uint16_t offset) {
    k_mutex_lock(&lockbox_mutex, K_FOREVER);
    char *passcode = g_lockbox_state.current_passcode;
    k_mutex_unlock(&lockbox_mutex);
    
    return bt_gatt_attr_read(conn, attr, buf, len, offset, passcode, strlen(passcode));
}

// NEW: VOC sensor characteristic handlers
static ssize_t write_voc_sensor(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    if (len != 2) return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    
    // VOC data is sent as 16-bit value (little endian)
    uint16_t voc_value = *(uint16_t*)buf;
    
    process_voc_reading(voc_value);
    
    LOG_INF("BLE: VOC data received: %u PPB", voc_value);
    return len;
}

static ssize_t read_voc_sensor(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                              void *buf, uint16_t len, uint16_t offset) {
    struct {
        uint16_t current_voc;
        uint16_t threshold;
        uint32_t timestamp;
    } voc_data;
    
    voc_data.current_voc = current_voc_value;
    voc_data.threshold = VOC_PRESENCE_THRESHOLD;
    voc_data.timestamp = last_voc_timestamp;
    
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &voc_data, sizeof(voc_data));
}

// set up for observing thingy
static const bt_addr_le_t mac_addr_thing = {
    .type = BT_ADDR_LE_RANDOM,
    .a = {.val = {0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xDA}}  // Reversed from string
};

// found thingy
static void device_found(
    const bt_addr_le_t *addr, 
    int8_t rssi, 
    uint8_t type, 
    struct net_buf_simple *ad
) {
    
    bool is_mac_thing = (bt_addr_le_cmp(addr, &mac_addr_thing) == 0);

    if (is_mac_thing) { 
        // VOC data is sent as 16-bit value (little endian)

        uint8_t *data = ad->data;

        uint16_t voc_value = (data[25] << 8) | data[26];
        
        process_voc_reading(voc_value);
        
        LOG_INF("BLE: VOC data received: %d PPB", voc_value);

        return voc_value;
    }
}

void observer_start(void)
{
  struct bt_le_scan_param scan_param = {
  .type       = BT_LE_SCAN_TYPE_PASSIVE,
  .options    = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
  .interval   = BT_GAP_SCAN_FAST_INTERVAL,
  .window     = BT_GAP_SCAN_FAST_WINDOW,
  };
  bt_le_scan_start(&scan_param, device_found);
}

BT_GATT_SERVICE_DEFINE(lockbox_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_LOCKBOX_SERVICE),
    
    BT_GATT_CHARACTERISTIC(BT_UUID_USERNAME, 
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
        read_username, write_username, NULL),
        
    BT_GATT_CHARACTERISTIC(BT_UUID_BLOCK_INFO,
        BT_GATT_CHRC_READ,
        BT_GATT_PERM_READ,
        read_block_info, NULL, NULL),
        
    BT_GATT_CHARACTERISTIC(BT_UUID_LOCK_STATUS,
        BT_GATT_CHRC_READ,
        BT_GATT_PERM_READ,
        read_lock_status, NULL, NULL),
        
    BT_GATT_CHARACTERISTIC(BT_UUID_USER_STATUS,
        BT_GATT_CHRC_READ,
        BT_GATT_PERM_READ,
        read_user_status, NULL, NULL),
        
    BT_GATT_CHARACTERISTIC(BT_UUID_PASSCODE,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
        read_passcode, write_passcode, NULL),
        
    // NEW: VOC sensor characteristic
    BT_GATT_CHARACTERISTIC(BT_UUID_VOC_SENSOR,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
        read_voc_sensor, write_voc_sensor, NULL),
);

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_NAME_COMPLETE, "SecureLockbox"),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x34, 0x12),
};

static bool validate_block(simple_block_t *block) {
    uint32_t calculated_hash = calculate_block_hash(block);
    if (calculated_hash != block->block_hash) {
        LOG_ERR("Block %u hash mismatch", block->id);
        return false;
    }
    
    if (block->block_hash > 0x0000FFFF) {
        LOG_ERR("Block %u invalid proof of work", block->id);
        return false;
    }
    
    for (int i = 0; i < block->tx_count; i++) {
        simple_transaction_t *tx = &block->transactions[i];
        uint32_t tx_hash = simple_hash(tx, sizeof(simple_transaction_t) - sizeof(tx->hash));
        if (tx_hash != tx->hash) {
            LOG_ERR("Transaction %d in block %u has invalid hash", i, block->id);
            return false;
        }
    }
    
    return true;
}

static int validate_blockchain(void) {
    LOG_INF("Validating blockchain...");
        
    for (uint32_t i = 0; i < g_blockchain.total_blocks; i++) {
        simple_block_t *block = &g_blockchain.blocks[i];
        
        if (!validate_block(block)) {
            LOG_ERR("Block %u validation failed", block->id);
            return -1;
        }
        
        if (i > 0) {
            simple_block_t *prev_block = &g_blockchain.blocks[i - 1];
            if (block->prev_hash != prev_block->block_hash) {
                LOG_ERR("Block %u has invalid previous hash", block->id);
                return -1;
            }
        }
    }
    
    LOG_INF("Blockchain validation successful");
    return 0;
}

static void connected(struct bt_conn *conn, uint8_t err) {
    if (err) {
        LOG_ERR("BLE Connection failed (err 0x%02x)", err);
        return;
    }
    
    current_conn = bt_conn_ref(conn);
    LOG_INF("BLE PC Connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
    LOG_INF("BLE PC Disconnected (reason 0x%02x)", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};
=======
SHELL_CMD_REGISTER(lockbox, &lockbox_cmds, "Lockbox commands", NULL);
>>>>>>> 2179525d9a6bc5e21907a6b95a84eec0f3b3968c

// Main function
int main(void) {
    int err;

    bt_addr_le_t mobile_addr;
    bt_addr_le_from_str(static_mac_str, "random", &mobile_addr);

    err = bt_id_create(&mobile_addr, NULL);
    if (err < 0) {
        LOG_ERR("Failed to set static address");
        return -1;
    }

    LOG_INF("Continuous BLE Lockbox System Starting...");
    
    // Initialize filesystem
    struct fs_statvfs stats;
    int ret = fs_statvfs("/lfs", &stats);
    if (ret != 0) {
        ret = fs_mount(&blockchain_fs_mount);
        if (ret < 0) {
            LOG_ERR("Failed to mount filesystem: %d", ret);
            return ret;
        }
    }
    
    // Initialize mutexes and queues
    k_mutex_init(&blockchain_mutex);
    k_mutex_init(&lockbox_mutex);
    k_mutex_init(&connections_mutex);
    k_msgq_init(&pending_tx_queue, (char *)pending_tx_buffer,
                sizeof(simple_transaction_t), 10);
    
    // Initialize states
    memset(&g_lockbox_state, 0, sizeof(lockbox_state_t));
    g_lockbox_state.state = STATE_READY;
    memset(connections, 0, sizeof(connections));
    
    // Load blockchain
    ret = load_blockchain();
    if (ret < 0) {
        LOG_ERR("Failed to initialize blockchain: %d", ret);
        return ret;
    }
    
    // Initialize lock
    lock_init();
    
    LOG_INF("Blockchain initialized with %u blocks", g_blockchain.total_blocks);

    // Start Bluetooth
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return -1;
    }
    
    LOG_INF("BLE Multi-Connection Lockbox ready");
    LOG_INF("- VOC sensor will send data every 2 seconds");
    LOG_INF("- VOC > %u PPB triggers user-independent presence detection", VOC_PRESENCE_THRESHOLD);
    LOG_INF("- PC will receive continuous notifications");
    LOG_INF("- Flow: VOC trigger -> Username -> Passcode -> Unlock");
    LOG_INF("- Supports up to %d simultaneous connections", MAX_BLE_CONNECTIONS);

    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Advertising failed (err %d)", err);
        return -1;
    }

    LOG_INF("Advertising started - ready for connections");
    
    observer_start();

    while (1) {
        k_sleep(K_MSEC(5000));
        LOG_INF("System running - VOC: %u PPB, Blocks: %u", 
                current_voc_value, g_blockchain.total_blocks);
    }
    
    return 0;
}