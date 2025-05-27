
/*
 * Copyright (c) 2020 Christian Taedcke
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

// From sams helpful ed post #178

// Configuring this sensor nodes supply_gpio 
// also configures all the other sensor nodes supply_gpio
// because they use the same supply_gpio.

struct supply_cfg {
    const struct device *gpio;
    gpio_pin_t pin;
    gpio_dt_flags_t flags;
};

static int enable_supply(const struct supply_cfg *cfg)
{
    int rv = -ENODEV;

    if (device_is_ready(cfg->gpio)) {
        gpio_pin_configure(cfg->gpio, cfg->pin,
                   GPIO_OUTPUT | cfg->flags);
        gpio_pin_set(cfg->gpio, cfg->pin, 1);
        rv = 0;
    }

    return rv;
}

static int efr32mg_sltb004a_init(void)
{
    struct supply_cfg cfg;
    int rc = 0;

    (void)cfg;

#define BMP280 DT_NODELABEL(bmp280)
#if DT_NODE_HAS_STATUS_OKAY(BMP280)
    cfg = (struct supply_cfg){
        .gpio = DEVICE_DT_GET(DT_GPIO_CTLR(BMP280, supply_gpios)),
        .pin = DT_GPIO_PIN(BMP280, supply_gpios),
        .flags = DT_GPIO_FLAGS(BMP280, supply_gpios),
    };

    /* Enable the ENV_I2C power domain */
    rc = enable_supply(&cfg);
    if (rc < 0) {
        printk("BMP280 supply not enabled: %d\n", rc);
    }
#endif

    return rc;
}

/* needs to be done after GPIO driver init */
SYS_INIT(efr32mg_sltb004a_init, POST_KERNEL,
     CONFIG_KERNEL_INIT_PRIORITY_DEVICE);