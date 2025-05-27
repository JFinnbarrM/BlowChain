
#include <zephyr/drivers/sensor_data_types.h>

// Functions that allow access to thread data
int si7021_get_temp_data(struct sensor_q31_data* data);
int si7021_get_hume_data(struct sensor_q31_data* data);

void si7021_get_tempstamp(char* tempstamp, struct sensor_q31_data* temp_data);
void si7021_get_humestamp(char* humestamp, struct sensor_q31_data* hume_data);

void si7021_printk_temp(struct sensor_q31_data* temp_data);
void si7021_printk_hume(struct sensor_q31_data* hume_data);

void si7021_printk_temp_shell(struct sensor_q31_data* temp_data);
void si7021_printk_hume_shell(struct sensor_q31_data* hume_data);