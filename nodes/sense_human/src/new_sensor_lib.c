#include "new_sensor_lib.h"

const struct device *const gas = DEVICE_DT_GET_ONE(ams_ccs811);

double convert_and_collect(const struct device * dev, enum sensor_channel chan);

struct s_data {
	void *lifo_reserved; /* 1st word reserved for use by lifo */
	double value;
    char* sensor;
    double all[4];
};

struct sensor_values {
    int did;
    char* time;
    char* *values;
    int count;
};

double convert_and_collect(const struct device * dev, enum sensor_channel chan) {
    struct sensor_value s;
    if (sensor_sample_fetch(dev) < 0) {
		return -1;
	}

    if (sensor_channel_get(dev, chan, &s) < 0) {
        return -1;
    }
    double val = sensor_value_to_double(&s);
    return val;
}