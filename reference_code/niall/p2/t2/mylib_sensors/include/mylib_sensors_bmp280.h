
#include <zephyr/drivers/sensor_data_types.h>

// Functions that allow access to thread data
int bmp280_get_temp_data(struct sensor_q31_data* data);
int bmp280_get_pres_data(struct sensor_q31_data* data);

void bmp280_get_tempstamp(char* tempstamp, struct sensor_q31_data* temp_data);
void bmp280_get_presstamp(char* presstamp, struct sensor_q31_data* pres_data);

void bmp280_printk_temp(struct sensor_q31_data* temp_data);
void bmp280_printk_pres(struct sensor_q31_data* pres_data);

void bmp280_printk_temp_shell(struct sensor_q31_data* temp_data);
void bmp280_printk_pres_shell(struct sensor_q31_data* pres_data);
