
#include "zephyr/drivers/sensor.h"
#include <mylib_sensors_bmp280.h>
#include <mylib_sensors_si7021.h>
#include <mylib_sensors_si1133.h>

#include <zephyr/kernel.h>

#include <zephyr/drivers/sensor.h>

static const struct device* si1133dev = DEVICE_DT_GET(DT_NODELABEL(si1133));

int main(void)
{
    while (1) {

        struct sensor_q31_data bmp280_temp_data;
        bmp280_get_temp_data(&bmp280_temp_data);
        bmp280_printk_temp(&bmp280_temp_data);

        struct sensor_q31_data bmp280_pres_data;
        bmp280_get_pres_data(&bmp280_pres_data);
        bmp280_printk_pres(&bmp280_pres_data);


        struct sensor_q31_data si7021_temp_data;
        si7021_get_temp_data(&si7021_temp_data);
        si7021_printk_temp(&si7021_temp_data);

        struct sensor_q31_data si7021_hume_data;
        si7021_get_hume_data(&si7021_hume_data);
        si7021_printk_hume(&si7021_hume_data);

        printk("-----------------------------\n");

        k_sleep(K_MSEC(1000));
	}
	return 0;
}