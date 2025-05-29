#ifndef LEDS
#define LEDS

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <stdlib.h>
#include <stdio.h>
#include <zephyr/drivers/led.h>

extern void set_colour(int r, int g, int b);

#endif