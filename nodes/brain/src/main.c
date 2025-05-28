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

static char* static_mac_str = "dc:80:00:00:08:35";

#define MAX_TRANSACTIONS_PER_BLOCK 3
#define MAX_USER_ID_LEN 16
#define MAX_DATA_LEN 32
#define BLOCKCHAIN_FILE "/lfs/chain.dat"
#define MAX_BLOCKS 20
#define HASH_SIZE 8
#define MAX_FAILED_ATTEMPTS 3
#define PASSCODE_LEN 6

#define BT_UUID_LOCKBOX_SERVICE \
    BT_UUID_DECLARE_128(0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc)
#define BT_UUID_USERNAME \
    BT_UUID_DECLARE_128(0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01)
#define BT_UUID_BLOCK_INFO \
    BT_UUID_DECLARE_128(0xBB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02)
#define BT_UUID_LOCK_STATUS \
    BT_UUID_DECLARE_128(0xCC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03)
#define BT_UUID_USER_STATUS \
    BT_UUID_DECLARE_128(0xDD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04)
#define BT_UUID_PASSCODE \
    BT_UUID_DECLARE_128(0xEE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05)

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

static blockchain_data_t g_blockchain;
static lockbox_state_t g_lockbox_state = {0};
static struct k_mutex blockchain_mutex;
static struct k_mutex lockbox_mutex;
static struct k_msgq pending_tx_queue;
static simple_transaction_t pending_tx_buffer[10];
static struct bt_conn *current_conn = NULL;
static char current_username[MAX_USER_ID_LEN] = "UNKNOWN";

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
    
    LOG_INF("Block %u mined! Nonce: %u, Hash: 0x%08x", 
            block->id, block->nonce, block->block_hash);
}

static int save_blockchain(void) {
    struct fs_file_t file;
    int ret;
    
    memset(&file, 0, sizeof(file));
    
    ret = fs_open(&file, BLOCKCHAIN_FILE, FS_O_CREATE | FS_O_WRITE);
    if (ret < 0) {
        LOG_ERR("Failed to open blockchain file: %d", ret);
        fs_close(&file);
        return ret;
    }
    
    ret = fs_write(&file, &g_blockchain, sizeof(blockchain_data_t));
    if (ret < 0) {
        LOG_ERR("Failed to write blockchain: %d", ret);
    } else {
        LOG_INF("Blockchain saved (%d bytes)", ret);
    }
    
    fs_close(&file);
    return ret;
}

static int load_blockchain(void) {
    struct fs_file_t file;
    int ret;
    
    memset(&file, 0, sizeof(file));
    
    ret = fs_open(&file, BLOCKCHAIN_FILE, FS_O_READ);
    if (ret < 0) {
        LOG_WRN("No existing blockchain file, creating new one");
        fs_close(&file);
        return create_genesis_block();
    }
    
    ret = fs_read(&file, &g_blockchain, sizeof(blockchain_data_t));
    if (ret < 0) {
        LOG_ERR("Failed to read blockchain: %d", ret);
        fs_close(&file);
        return ret;
    } else {
        LOG_INF("Loaded blockchain: %u blocks", g_blockchain.total_blocks);
    }
    
    fs_close(&file);
    return 0;
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
    g_lockbox_state.state = STATE_READY;
    g_lockbox_state.failed_attempts = 0;
    
    k_mutex_unlock(&lockbox_mutex);
    
    char data[MAX_DATA_LEN];
    snprintf(data, sizeof(data), "Passcode: %s", g_lockbox_state.current_passcode);
    add_transaction(TX_PASSCODE_GENERATED, user_id, data);
    
    LOG_INF("Passcode generated for %s: %s", user_id, g_lockbox_state.current_passcode);
}

static void detect_presence(const char *user_id) {
    k_mutex_lock(&lockbox_mutex, K_FOREVER);
    
    if (g_lockbox_state.state == STATE_READY) {
        g_lockbox_state.state = STATE_PRESENCE_DETECTED;
        LOG_INF("Presence detected for %s", user_id);
        
        k_mutex_unlock(&lockbox_mutex);
        add_transaction(TX_PRESENCE_DETECTED, user_id, "Human presence detected");
        
        k_mutex_lock(&lockbox_mutex, K_FOREVER);
        g_lockbox_state.state = STATE_WAITING_PASSCODE;
        k_mutex_unlock(&lockbox_mutex);
        
        LOG_INF("System ready for passcode entry");
    } else {
        k_mutex_unlock(&lockbox_mutex);
        LOG_WRN("Presence detected but system not ready");
    }
}

static bool verify_passcode(const char *user_id, const char *entered_code) {
    k_mutex_lock(&lockbox_mutex, K_FOREVER);
    
    if (g_lockbox_state.state != STATE_WAITING_PASSCODE) {
        k_mutex_unlock(&lockbox_mutex);
        LOG_WRN("Passcode entry attempted but system not waiting");
        return false;
    }
    
    bool success = (strcmp(entered_code, g_lockbox_state.current_passcode) == 0);
    
    if (success) {
        g_lockbox_state.state = STATE_READY;
        g_lockbox_state.failed_attempts = 0;
        k_mutex_unlock(&lockbox_mutex);
        
        add_transaction(TX_PASSCODE_VERIFIED, user_id, "Access granted");
        LOG_INF("Access granted for %s", user_id);
        return true;
    } else {
        g_lockbox_state.failed_attempts++;
        LOG_WRN("Failed passcode attempt %u/3 for %s", g_lockbox_state.failed_attempts, user_id);
        
        char data[MAX_DATA_LEN];
        snprintf(data, sizeof(data), "Failed attempt %u/3", g_lockbox_state.failed_attempts);
        
        if (g_lockbox_state.failed_attempts >= MAX_FAILED_ATTEMPTS) {
            g_lockbox_state.state = STATE_LOCKED;
            g_lockbox_state.system_locked = true;
            k_mutex_unlock(&lockbox_mutex);
            
            add_transaction(TX_SYSTEM_LOCKED, user_id, "System locked");
            send_alert_to_dashboard("ALERT: Maximum failed attempts reached - System locked");
            LOG_ERR("System locked due to failed attempts");
        } else {
            g_lockbox_state.state = STATE_READY;
            k_mutex_unlock(&lockbox_mutex);
            add_transaction(TX_PASSCODE_FAILED, user_id, data);
        }
        return false;
    }
}

static void trigger_tamper_alert(void) {
    k_mutex_lock(&lockbox_mutex, K_FOREVER);
    g_lockbox_state.tamper_detected = true;
    g_lockbox_state.system_locked = true;
    g_lockbox_state.state = STATE_LOCKED;
    k_mutex_unlock(&lockbox_mutex);
    
    add_transaction(TX_TAMPER_DETECTED, "SYSTEM", "Tamper alert - lockbox compromised");
    send_alert_to_dashboard("CRITICAL: Tamper detected - Emergency lockdown");
    LOG_ERR("TAMPER DETECTED - EMERGENCY LOCKDOWN");
}

static void send_alert_to_dashboard(const char *message) {
    LOG_ERR("DASHBOARD ALERT: %s", message);
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

static int cmd_status(const struct shell *sh, size_t argc, char **argv) {
    k_mutex_lock(&lockbox_mutex, K_FOREVER);
    k_mutex_lock(&blockchain_mutex, K_FOREVER);
    
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
    
    shell_print(sh, "\n=== Blockchain Status ===");
    shell_print(sh, "Total Blocks: %u", g_blockchain.total_blocks);
    shell_print(sh, "Latest Hash: 0x%08x", g_blockchain.latest_hash);
    
    k_mutex_unlock(&blockchain_mutex);
    k_mutex_unlock(&lockbox_mutex);
    
    return 0;
}

static int cmd_generate_passcode(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 2) {
        shell_error(sh, "Usage: generate_passcode <user_id>");
        return -EINVAL;
    }
    
    generate_passcode(argv[1]);
    shell_print(sh, "Passcode generated for user: %s", argv[1]);
    return 0;
}

static int cmd_detect_presence(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 2) {
        shell_error(sh, "Usage: detect_presence <user_id>");
        return -EINVAL;
    }
    
    detect_presence(argv[1]);
    shell_print(sh, "Presence detected for user: %s", argv[1]);
    return 0;
}

static int cmd_enter_passcode(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 3) {
        shell_error(sh, "Usage: enter_passcode <user_id> <passcode>");
        return -EINVAL;
    }
    
    bool success = verify_passcode(argv[1], argv[2]);
    shell_print(sh, "Passcode verification: %s", success ? "SUCCESS" : "FAILED");
    if (success == 1) {
        lock_open();
    } else {
        lock_close();
    }
    return 0;
}

static int cmd_trigger_tamper(const struct shell *sh, size_t argc, char **argv) {
    trigger_tamper_alert();
    shell_print(sh, "Tamper alert triggered - system locked");
    lock_close();
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
    shell_print(sh, "Type 'YES' to confirm reset:");
    
    if (argc > 1 && strcmp(argv[1], "YES") == 0) {
        int ret = reset_blockchain();
        if (ret == 0) {
            shell_print(sh, "Blockchain reset successfully");
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

SHELL_STATIC_SUBCMD_SET_CREATE(lockbox_cmds,
    SHELL_CMD(status, NULL, "Show lockbox and blockchain status", cmd_status),
    SHELL_CMD(generate_passcode, NULL, "Generate passcode for user", cmd_generate_passcode),
    SHELL_CMD(detect_presence, NULL, "Simulate presence detection", cmd_detect_presence),
    SHELL_CMD(enter_passcode, NULL, "Enter passcode for verification", cmd_enter_passcode),
    SHELL_CMD(trigger_tamper, NULL, "Trigger tamper alert", cmd_trigger_tamper),
    SHELL_CMD(blockchain_stats, NULL, "Show blockchain statistics", cmd_blockchain_stats),
    SHELL_CMD(history, NULL, "Show user transaction history", cmd_history),
    SHELL_CMD(reset, NULL, "Reset blockchain (WARNING: Destructive!)", cmd_reset),
    SHELL_SUBCMD_SET_END
);

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
);

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_NAME_COMPLETE, "SecureLockbox"),
    BT_DATA(BT_DATA_UUID128_ALL, BT_UUID_LOCKBOX_SERVICE, 16),
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

int main(void) {
    int err;

    bt_addr_le_t mobile_addr;
    bt_addr_le_from_str(static_mac_str, "random", &mobile_addr);

    err = bt_id_create(&mobile_addr, NULL);
    if (err < 0) {
        LOG_ERR("Failed to set static address as identity (ERROR: %d)", err);
        return -1;
    } else {
        LOG_INF("Static address (%s) set as identity", static_mac_str);
    }

    LOG_INF("BlowChain Lockbox System Starting...");
    lock_init();
    
    struct fs_statvfs stats;
    int ret = fs_statvfs("/lfs", &stats);
    if (ret == 0) {
        LOG_INF("Filesystem already mounted (auto-mount)");
    } else {
        ret = fs_mount(&blockchain_fs_mount);
        if (ret < 0) {
            LOG_ERR("Failed to mount filesystem: %d", ret);
            return ret;
        }
        LOG_INF("Filesystem mounted manually");
    }
    
    k_mutex_init(&blockchain_mutex);
    k_mutex_init(&lockbox_mutex);
    k_msgq_init(&pending_tx_queue, (char *)pending_tx_buffer,
                sizeof(simple_transaction_t), 10);
    
    memset(&g_lockbox_state, 0, sizeof(lockbox_state_t));
    g_lockbox_state.state = STATE_READY;
    
    ret = load_blockchain();
    if (ret < 0) {
        LOG_ERR("Failed to initialize blockchain: %d", ret);
        return ret;
    }
    
    if (validate_blockchain() < 0) {
        LOG_ERR("Blockchain validation failed!");
        return -1;
    }
    
    LOG_INF("BlowChain initialized successfully");
    LOG_INF("Blockchain has %u blocks", g_blockchain.total_blocks);
    LOG_INF("Use 'lockbox status' to check system status");
    LOG_INF("Use 'lockbox generate_passcode <user>' to start workflow");

    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return -1;
    }
    LOG_INF("BLE Lockbox ready for PC connection");

    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Advertising failed (err %d)", err);
        return -1;
    }

    LOG_INF("Advertising successfully started - PC can now connect");
    
    while (1) {
        k_mutex_lock(&lockbox_mutex, K_FOREVER);
        if (g_lockbox_state.state == STATE_SHUTDOWN) {
            k_mutex_unlock(&lockbox_mutex);
            LOG_ERR("System shutdown initiated");
            break;
        }
        k_mutex_unlock(&lockbox_mutex);
        
        k_sleep(K_MSEC(10000));
    }
    
    return 0;
}