

#include "../include/controller.h"

#include <stdint.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>
#include <stdbool.h>

LOG_MODULE_REGISTER(controller);

// Defines a static MAC address
static char* static_mac_str = "dc:78:58:8d:01:13";

void bt_init(void) {
    int err;

	// Create the MAC address from the string.
	bt_addr_le_t mobile_addr;
	bt_addr_le_from_str(static_mac_str, "random", &mobile_addr);

	// Set the identity address to the MAC address.
	err = bt_id_create(&mobile_addr, NULL);
    if (err < 0) {
        LOG_ERR("Failed to set static address as identity (ERROR: %d)", err);
        return;
    } else {
        LOG_INF("Static address (%s) set as identity", static_mac_str);
    }

    // Initialize the Bluetooth subsystem
    err = bt_enable(NULL);
    if (err != 0) {
        LOG_ERR("Bluetooth initialisation failed (err %d)", err);
        return;
    } else {
        LOG_INF("Bluetooth initialised");
    }
}

void controller_tfun(void)
{
    bt_init();

    // Get and set data doing calculations on it.
}

K_THREAD_DEFINE(            // Static thread declaration.
    controller_tid,         // Thread ID.
    1024,                   // Memory (1KB).
    controller_tfun,        // Entry point.
    NULL, NULL, NULL,       // No parameters.
    1,                      // Priority.
    0,                      // No special options.
    0                       // Start immediately.
);   