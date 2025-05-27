

#include <zephyr/drivers/gpio.h>

// Fetch the node identifiers from /aliases.
#define LED1_NODE DT_ALIAS(led2)
#define LED2_NODE DT_ALIAS(led0)

// Create containers of node information from the device tree nodes.
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET_BY_IDX(LED1_NODE, gpios, 0);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET_BY_IDX(LED2_NODE, gpios, 0);