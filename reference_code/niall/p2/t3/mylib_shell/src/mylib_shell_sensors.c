
#include <mylib_sensors_bmp280.h>
#include <mylib_sensors_si1133.h>
#include <mylib_sensors_si7021.h>

#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include <string.h>

static int handle_get_temp(const struct shell *sh, size_t argc, char **argv) 
{
    struct sensor_q31_data si7021_temp_data;
    si7021_get_temp_data(&si7021_temp_data);
    si7021_printk_temp(&si7021_temp_data);

    struct sensor_q31_data bmp280_temp_data;
    bmp280_get_temp_data(&bmp280_temp_data);
    bmp280_printk_temp(&bmp280_temp_data);
    
    return 0;
}

static int handle_get_hume(const struct shell *sh, size_t argc, char **argv) 
{
    struct sensor_q31_data hume_data;
    si7021_get_hume_data(&hume_data);
    si7021_printk_hume(&hume_data);
    return 0;
}

static int handle_get_pres(const struct shell *sh, size_t argc, char **argv) 
{
    struct sensor_q31_data pres_data;
    bmp280_get_pres_data(&pres_data);
    bmp280_printk_pres(&pres_data);
    return 0;
}

static int handle_get_lite(const struct shell *sh, size_t argc, char **argv) 
{
    printk("How about YOU make a zephyr driver for this <REDACTED> stupid sensor.");
    return 0;
}

static int handle_get_all(const struct shell *sh, size_t argc, char **argv) 
{
    handle_get_temp(sh, 0, 0);
    handle_get_hume(sh, 0, 0);
    handle_get_pres(sh, 0, 0);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sensor_cmds,
    SHELL_CMD_ARG(0, NULL, "Get the temperature",          &handle_get_temp, 1, 0),
    SHELL_CMD_ARG(1, NULL, "Get the humidity",             &handle_get_hume, 1, 0),
    SHELL_CMD_ARG(2, NULL, "Get the air pressure",         &handle_get_pres, 1, 0),
    SHELL_CMD_ARG(5, NULL, "Get the ambient light",        &handle_get_lite, 1, 0),
    SHELL_CMD_ARG(15, NULL, "Get all sensor measurments.", &handle_get_all,  1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sensor, &sensor_cmds, "Get a measurement from the selected sensor", NULL);