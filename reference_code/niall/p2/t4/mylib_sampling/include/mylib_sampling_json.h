

#include <zephyr/drivers/sensor_data_types.h>
#include <zephyr/drivers/rtc.h>

void jsonify_samples(struct rtc_time* rtc_time_ptr, 
    struct sensor_q31_data *data_array, int *did_array, size_t count);