#include "new_sensor_lib.h"

const struct device *const magneto = DEVICE_DT_GET_ONE(st_lis3mdl_magn);
const struct device *const accel = DEVICE_DT_GET_ONE(st_lsm6dsl);

struct sensor_data {
    int x;
    int y;
    int z;
	int xf;
    int yf;
    int zf;
};

// struct sensor_data convert_and_collect(const struct device * dev, enum sensor_channel channel);

struct sensor_data convert_and_collect(const struct device *sensor,
			   enum sensor_channel channel)
{
	struct sensor_value val[3];
	static struct sensor_value accel_x, accel_y, accel_z;
	int32_t ret = 0;

    struct sensor_data sd = {0};

	double xd;
	double yd;
	double zd;



	if (channel == SENSOR_CHAN_ACCEL_XYZ) {
		struct sensor_value odr_attr;
		odr_attr.val1 = 104;
		odr_attr.val2 = 0;
		if (sensor_attr_set(sensor, SENSOR_CHAN_ACCEL_XYZ,
					SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) {
			printk("Cannot set sampling frequency for accelerometer.\n");
		}


		sensor_sample_fetch_chan(sensor, SENSOR_CHAN_ACCEL_XYZ);
		sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_X, &accel_x);
		sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_Y, &accel_y);
		sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_Z, &accel_z);

		xd = sensor_value_to_double(&accel_x);
		yd = sensor_value_to_double(&accel_y);
		zd = sensor_value_to_double(&accel_z);
	}
	
	else {
		ret = sensor_sample_fetch(sensor);
		if (ret < 0 && ret != -EBADMSG) {
			printf("Sensor sample update error\n");
			return sd;
		}

		ret = sensor_channel_get(sensor, channel, val);
		if (ret < 0) {
			printf("Cannot read sensor channels\n");
			return sd;
		}
		
		xd = sensor_value_to_double(&val[0]);
		yd = sensor_value_to_double(&val[1]);
		zd = sensor_value_to_double(&val[2]);
	}


	int x = (int) xd;
	int xf = (int)((xd - x) * 100);
	if (xf < 0) xf = -xf;


	int y = (int) yd;
	int yf = (int)((yd - y) * 100);
	if (yf < 0) yf = -yf;

	int z = (int) zd;
	int zf = (int)((zd - z) * 100);
	if (zf < 0) zf = -zf;

    struct sensor_data s = {
		.x = x,
		.y = y,
		.z = z,
		.xf = xf,
		.yf = yf,
		.zf = zf
    };

	// printf("( x y z ) = ( %f  %f  %f )\n", xd,
	// 					yd,
	// 					zd);

	return s;
}