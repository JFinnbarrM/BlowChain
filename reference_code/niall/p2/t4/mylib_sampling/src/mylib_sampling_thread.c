
#include "mylib_sampling_thread.h"
#include "mylib_sampling_button.h"
#include "mylib_sampling_json.h"

#include <mylib_clock_rtcc.h>

#include <mylib_sensors_bmp280.h>
#include <mylib_sensors_si1133.h>
#include <mylib_sensors_si7021.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/drivers/sensor_data_types.h>

#include <zephyr/data/json.h>

static ATOMIC_DEFINE(a_sampling, 1);
K_SEM_DEFINE(sample_sem, 0, 1);

int sampling_state(void) {
    return atomic_get(a_sampling);
}

int sampling_set_on(void) {
    printk("Sampling: ON\n");
    atomic_set_bit_to(a_sampling, 0, true);
    k_sem_reset(&sample_sem); // Dont allow excess gives.
    k_sem_give(&sample_sem);
    return 0;
}

int sampling_set_off(void) {
    printk("Sampling: OFF\n");
    atomic_set_bit_to(a_sampling, 0, false);
    return 0;
}

atomic_t a_freq = ATOMIC_INIT(2);

int sampling_set_frequency(int freq) {
    atomic_set(&a_freq, freq);
    return 0;
}

atomic_t a_sensor_select[ATOMIC_BITMAP_SIZE(5)];

void sample_did_set(uint8_t did, bool set_to)
{
    if (did==ALL) {
        atomic_val_t set = set_to ? 31 : 0;
        atomic_set(a_sensor_select, set);
    } else {
        atomic_set_bit_to(a_sensor_select, did, set_to);
    }
}

static void take_samples(void) 
{
    struct rtc_time time_data;
    rtcc_get_rtc_time(&time_data);
    rtcc_log_rtc_time(&time_data);

    struct sensor_q31_data sensor_data_array[6];
    int did_array[6];
    size_t count = 0;

    if (atomic_test_bit(a_sensor_select, TEMP)) {
        struct sensor_q31_data temp_data;
        si7021_get_temp_data(&temp_data);
        si7021_printk_temp(&temp_data);

        sensor_data_array[count] = temp_data;
        did_array[count] = TEMP;
        count++;
    }

    if (atomic_test_bit(a_sensor_select, HUME)) {
        struct sensor_q31_data hume_data;
        si7021_get_hume_data(&hume_data);
        si7021_printk_hume(&hume_data);

        sensor_data_array[count] = hume_data;
        did_array[count] = HUME;
        count++;
    }

    if (atomic_test_bit(a_sensor_select, PRES)) {
        struct sensor_q31_data pres_data;
        bmp280_get_pres_data(&pres_data);
        bmp280_printk_pres(&pres_data);

        sensor_data_array[PRES] = pres_data;
        did_array[count] = PRES;
        count++;
    }

    if (atomic_test_bit(a_sensor_select, LITE)) {
        // :(
    }

    jsonify_samples(&time_data, 
       sensor_data_array, did_array, count);
}

static void sampler_tfun(void) 
{
    rtcc_setup();
    button_setup();

    int64_t reftime_ms;
    int64_t ms_break_remaining;

    while (1) {
        k_sem_take(&sample_sem, K_FOREVER);

        while(atomic_get(a_sampling)) {
            reftime_ms = k_uptime_get();
            
            take_samples();
            
            // More exact timings.
            ms_break_remaining = (atomic_get(&a_freq)*1000) - k_uptime_delta(&reftime_ms);
            printk("Taking a break for %llu ms\n", ms_break_remaining);
            k_sleep(K_MSEC(MAX(0, ms_break_remaining)));
        }
    }
}

K_THREAD_DEFINE(            // Static thread declaration.
    sampler_tid,            // Thread ID.
    10000,                  // Memory (10KB).
    sampler_tfun,           // Entry point.
    NULL, NULL, NULL,       // No parameters.
    1,                      // Priority.
    0,                      // No special options.
    0                       // Start immediately.
);   