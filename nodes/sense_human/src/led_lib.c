#include "led_lib.h"
#include "new_sensor_lib.h"

struct s_data_a {
	void *fifo_reserved; /* 1st word reserved for use by fifo */
	double value;
    char* sensor;
};

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)

#if !DT_NODE_HAS_STATUS_OKAY(LED0_NODE)
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

#if !DT_NODE_HAS_STATUS_OKAY(LED1_NODE)
#error "Unsupported board: led1 devicetree alias is not defined"
#endif

#if !DT_NODE_HAS_STATUS_OKAY(LED2_NODE)
#error "Unsupported board: led2 devicetree alias is not defined"
#endif

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);

void set_colour(int r, int g, int b) {
    gpio_pin_set_dt(&led0, r);
    gpio_pin_set_dt(&led1, g);
    gpio_pin_set_dt(&led2, b);
}