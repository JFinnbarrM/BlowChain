

#include "shell.h"
#include "broadcaster.h"
#include "zephyr/kernel.h"

#include <zephyr/sys/slist.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/shell/shell.h>
#include <ctype.h>
#include <stdlib.h>

// DEFINE LINKED LIST

struct ibeacon_node {
    char name;
    bt_addr_le_t mac_addr;
    uint16_t major;
    uint16_t minor;
    int16_t x_coord;
    int16_t y_coord;
    char left_neighbor;
    char right_neighbor;

    sys_snode_t node;

    int8_t rssi;
};

static sys_slist_t ib_nodes = SYS_SLIST_STATIC_INIT(&ib_nodes);
static K_MUTEX_DEFINE(ib_nodes_mutex);

// SEARCH FUNCTION

static struct ibeacon_node* get_node(char name) {
    struct ibeacon_node *n;

    k_mutex_lock(&ib_nodes_mutex, K_FOREVER);
    SYS_SLIST_FOR_EACH_CONTAINER(&ib_nodes, n, node) {
        if (n->name == name) {
            k_mutex_unlock(&ib_nodes_mutex);
            return n;
        }
    }

    k_mutex_unlock(&ib_nodes_mutex);
    return NULL;
}

// EXTERNAL ACCESS FUNCTIONS

int update_node_rssi(char name, int8_t rssi) {
    struct ibeacon_node *n = get_node(name);
    if (!n) return -1;

    k_mutex_lock(&ib_nodes_mutex, K_FOREVER);
    n->rssi = rssi;

    k_mutex_unlock(&ib_nodes_mutex);
    return 0;
}

int get_node_rssi(char name, int8_t *rssi_out) {
    struct ibeacon_node *n = get_node(name);
    if (!n) return -1;

    k_mutex_lock(&ib_nodes_mutex, K_FOREVER);
    *rssi_out = n->rssi;

    k_mutex_unlock(&ib_nodes_mutex);
    return 0;
}

int count_nodes(void) {
    struct ibeacon_node *n;
    uint8_t count = 0;

    k_mutex_lock(&ib_nodes_mutex, K_FOREVER);
    SYS_SLIST_FOR_EACH_CONTAINER(&ib_nodes, n, node) {
        count++;
    }

    k_mutex_unlock(&ib_nodes_mutex);
    return count;
}

int get_rssi_data(struct rssi_measurement *rssi_data) {
    struct ibeacon_node *n;
    uint8_t count = 0;
    
    k_mutex_lock(&ib_nodes_mutex, K_FOREVER);
    SYS_SLIST_FOR_EACH_CONTAINER(&ib_nodes, n, node) {
        rssi_data[count].name = n->name;
        rssi_data[count].rssi = n->rssi;
        count++;
    }

    k_mutex_unlock(&ib_nodes_mutex);
    return 0;
}

// COMMAND HELPERS

static bool is_valid_name(const char *s) {
    return s && strlen(s) == 1 && isalpha(s[0]);
}

// COMMAND FUNCTIONS

static int cmd_add_node(const struct shell *shell, size_t argc, char **argv) {
    if (argc < 9) {
        shell_error(shell, "Usage: add <name> <mac> <major> <minor> <x> <y> <left> <right>");
        return -1;
    }

    if (!is_valid_name(argv[1]) || !is_valid_name(argv[7]) || !is_valid_name(argv[8])) {
        shell_error(shell, "Names must be single characters (A-Z)");
        return -1;
    }

    char name = toupper(argv[1][0]);

    if (get_node(name)) {
        shell_error(shell, "Node %c already exists", name);
        return -1;
    }

    struct ibeacon_node *n = k_malloc(sizeof(struct ibeacon_node));
    if (!n) {
        shell_error(shell, "Memory allocation failed");
        return -1;
    }

    n->name = name;
    if (bt_addr_le_from_str(argv[2], "random", &n->mac_addr) != 0) {
        shell_error(shell, "Invalid MAC address");
        k_free(n);
        return -1;
    }

    n->major = strtoul(argv[3], NULL, 10);
    n->minor = strtoul(argv[4], NULL, 10);
    n->x_coord = strtol(argv[5], NULL, 10);
    n->y_coord = strtol(argv[6], NULL, 10);
    n->left_neighbor = toupper(argv[7][0]);
    n->right_neighbor = toupper(argv[8][0]);
    n->rssi = 0; // Initialize RSSI to 0

    k_mutex_lock(&ib_nodes_mutex, K_FOREVER);
    sys_slist_prepend(&ib_nodes, &n->node);
    k_mutex_unlock(&ib_nodes_mutex);

    shell_print(shell, "Added node %c", name);
    return 0;
}

static int cmd_remove_node(const struct shell *shell, size_t argc, char **argv) {
    if (argc < 2 || !is_valid_name(argv[1])) {
        shell_error(shell, "Usage: remove <name>");
        return -1;
    }

    char name = toupper(argv[1][0]);
    struct ibeacon_node *n, *prev = NULL;

    k_mutex_lock(&ib_nodes_mutex, K_FOREVER);
    SYS_SLIST_FOR_EACH_CONTAINER(&ib_nodes, n, node) {
        if (n->name == name) {
            sys_slist_remove(&ib_nodes,
                prev ? &prev->node : NULL,
                &n->node);
            k_free(n);
            shell_print(shell, "Removed node %c", name);
            k_mutex_unlock(&ib_nodes_mutex);
            return 0;
        }
        prev = n;
    }

    k_mutex_unlock(&ib_nodes_mutex);
    shell_error(shell, "Node %c not found", name);
    return -1;
}

static void print_node(const struct shell *shell, const struct ibeacon_node *n) {
    char mac[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(&n->mac_addr, mac, sizeof(mac));

    shell_print(shell,
        "Name: %-2c | MAC: %-17s | Major: %-5u | Minor: %-5u | X: %-5d Y: %-5d | Left: %-2c | Right: %-2c | RSSI: %-3i",
        n->name, mac, n->major, n->minor, n->x_coord, n->y_coord,
        n->left_neighbor, n->right_neighbor, n->rssi);
}

static int cmd_view_nodes(const struct shell *shell, size_t argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "-a") == 0) {
        struct ibeacon_node *n;
        
        k_mutex_lock(&ib_nodes_mutex, K_FOREVER);
        SYS_SLIST_FOR_EACH_CONTAINER(&ib_nodes, n, node) {
            print_node(shell, n);
        }
        
        k_mutex_unlock(&ib_nodes_mutex);
        return 0;
    }

    if (argc < 2 || !is_valid_name(argv[1])) {
        shell_error(shell, "Usage: view <name> or view -a");
        return -1;
    }

    char name = toupper(argv[1][0]);
    struct ibeacon_node *n = get_node(name);
    if (!n) {
        shell_error(shell, "Node %c not found", name);
        return -1;
    }

    print_node(shell, n);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(ib_cmds,
    SHELL_CMD(add, NULL, "Add node: add <A-Z> <mac> <major> <minor> <x> <y> <left> <right>", cmd_add_node),
    SHELL_CMD(remove, NULL, "Remove node: remove <A-Z>", cmd_remove_node),
    SHELL_CMD(view, NULL, "View node: view <A-Z> or -a", cmd_view_nodes),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ib, &ib_cmds, "iBeacon node management", NULL);
