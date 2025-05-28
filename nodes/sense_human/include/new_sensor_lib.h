#ifndef SENSORS_NEW
#define SENSORS_NEW

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/spsc_pbuf.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/sensor/ccs811.h>
#include <zephyr/data/json.h>

extern const struct device *const gas;
extern void gas_read_thread(void);

extern double convert_and_collect(const struct device * dev, enum sensor_channel chan);
#endif