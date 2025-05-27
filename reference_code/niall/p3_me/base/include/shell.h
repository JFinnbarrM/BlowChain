

#include <stdint.h>

// Stores an RSSI measurement using 2 bytes (char identifier and int8_t RSSI value).
struct rssi_measurement {
    char name;      // Single character identifier (A-M).
    int8_t rssi;    // RSSI value.
};

// Accessing functions
int update_node_rssi(char name, int8_t rssi);
int get_node_rssi(char name, int8_t *rssi_out);
int count_nodes(void);
int get_rssi_data(struct rssi_measurement *rssi_data);