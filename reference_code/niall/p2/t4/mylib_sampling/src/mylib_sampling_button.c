
// Inspired by: ./zephyr/samples/basic/button
// Using: https://github.com/zephyrproject-rtos/zephyr/blob/main/drivers/input/input_gpio_keys.c

#include "mylib_sampling_thread.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(button);

#define B0 DT_NODELABEL(button0)

static const struct gpio_dt_spec but = GPIO_DT_SPEC_GET_BY_IDX(B0, gpios, 0);
static struct gpio_callback button_cb_data;

static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("You are really pushing my buttons...\n");
    
    if (sampling_state()) {
        sampling_set_off();
    } else {
        sampling_set_on();
    }
}

int button_setup(void)
{
    int ret;

	if (!(ret =gpio_is_ready_dt(&but))) {
		LOG_ERR("Error %d: button device %s is not ready", 
            ret, but.port->name);
		return -1;
	}

	ret = gpio_pin_configure_dt(&but, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to configure %s pin %d",
		    ret, but.port->name, but.pin);
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&but, GPIO_INT_EDGE_FALLING);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d",
			ret, but.port->name, but.pin);
		return -1;
	}

	gpio_init_callback(
        &button_cb_data, 
        button_pressed, 
        BIT(but.pin)
    );
	gpio_add_callback(but.port, &button_cb_data);
    
	LOG_INF("Set up button at %s pin %d", but.port->name, but.pin);
    return 0;
}