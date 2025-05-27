
// Inspired by zephyr/samples/sensor/bme280

#include "mylib_sensors_si7021.h"

#include <mylib_shell_globalsh.h>

#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/dsp/print_format.h>
#include <zephyr/sys/spsc_lockfree.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(si7021_thread);

static const struct device* dev = DEVICE_DT_GET(DT_NODELABEL(si7021));
SENSOR_DT_READ_IODEV( // Sets channels for sensor type.
                      // So the reader and decoders know what they're looking at.
    si7021_iodev, 
    DT_NODELABEL(si7021),
	{SENSOR_CHAN_AMBIENT_TEMP , 0},
	{SENSOR_CHAN_HUMIDITY , 0}
);
RTIO_DEFINE(si7021_ctx, 1, 1); // https://docs.zephyrproject.org/latest/services/rtio/index.html

// SPSC used as a message storing/passing primitive as required by spec.
SPSC_DEFINE(si7021_temp_spsc, struct sensor_q31_data, 2);
SPSC_DEFINE(si7021_hume_spec, struct sensor_q31_data, 2);
K_SEM_DEFINE(si7021_readin_start, 0, 1);
K_SEM_DEFINE(si7021_readin_finish, 0, 1);

int si7021_get_temp_data(struct sensor_q31_data *data) {
    k_sem_give(&si7021_readin_start);
    k_sem_take(&si7021_readin_finish, K_FOREVER);

    if (!spsc_consumable(&si7021_temp_spsc)) return -1;
    *data = *spsc_consume(&si7021_temp_spsc);
    spsc_release(&si7021_temp_spsc);
    return 0;
}

int si7021_get_hume_data(struct sensor_q31_data *data) {
    k_sem_give(&si7021_readin_start);
    k_sem_take(&si7021_readin_finish, K_FOREVER);

    if (!spsc_consumable(&si7021_hume_spec)) return -1;
    *data = *spsc_consume(&si7021_hume_spec);
    spsc_release(&si7021_hume_spec);
    return 0;
}


void si7021_get_tempstamp(char* tempstamp, struct sensor_q31_data* temp_data)
{
    snprintf(tempstamp, 32,
        "%" PRIq(2) "",
        PRIq_arg(temp_data->readings[0].pressure, 2, temp_data->shift)
    );
}

void si7021_printk_temp(struct sensor_q31_data* temp_data) 
{
    char tempstamp[32];
    si7021_get_tempstamp(tempstamp, temp_data);
    printk("(si7021) temperature=\t%s (C)\n", tempstamp);
}

void si7021_printk_temp_shell(struct sensor_q31_data* temp_data) 
{
    char tempstamp[32];
    si7021_get_tempstamp(tempstamp, temp_data);
    shell_print(sh_global, "(si7021) temperature=\t%s (C)\n", tempstamp);
}


void si7021_get_humestamp(char* humestamp, struct sensor_q31_data* hume_data)
{
    snprintf(humestamp, 32,
        "%" PRIq(2) "",
        PRIq_arg(hume_data->readings[0].pressure, 2, hume_data->shift)
    );
}

void si7021_printk_hume(struct sensor_q31_data* hume_data) 
{
    char humestamp[32];
    si7021_get_humestamp(humestamp, hume_data);
    printk("(si7021) humidity=\t%s (%%)\n", humestamp);
}

void si7021_printk_hume_shell(struct sensor_q31_data* hume_data) 
{
    char humestamp[32];
    si7021_get_humestamp(humestamp, hume_data);
    shell_print(sh_global, "(si7021) humidity=\t%s (%%)\n", humestamp);
}


static int init() 
{
	if      (dev == NULL)           {LOG_ERR("Device not found."                   ); return -1;}
	else if (!device_is_ready(dev)) {LOG_ERR("\"%s\" not ready.",         dev->name); return -1;}
	else                            {LOG_INF("\"%s\" ready to read data.",dev->name); return 0;};
}

static int sensor_readin(void) 
{
    uint8_t buf[128];
    int rc = sensor_read(&si7021_iodev, &si7021_ctx, buf, 128);
    if (rc != 0) { 
        LOG_ERR("%s: sensor_read() failed: %d", dev->name, rc);
        return rc;
    }

    const struct sensor_decoder_api *decoder;
    rc = sensor_get_decoder(dev, &decoder);
    if (rc != 0) {
        LOG_ERR("%s: sensor_get_decode() failed: %d", dev->name, rc);
        return rc;
    }

    uint32_t temp_fit = 0;
    struct sensor_q31_data temp_data = {0};
    decoder->decode(buf,
        (struct sensor_chan_spec) {SENSOR_CHAN_AMBIENT_TEMP, 0},
        &temp_fit, 1, &temp_data);

    uint32_t hume_fit = 0;
    struct sensor_q31_data hume_data = {0};
    decoder->decode(buf,
            (struct sensor_chan_spec) {SENSOR_CHAN_HUMIDITY, 0},
            &hume_fit, 1, &hume_data);

    spsc_reset(&si7021_temp_spsc);
    // while (!spsc_acquirable(&si7021_temp_spsc)) { k_sleep(K_MSEC(10)); }
    struct sensor_q31_data* temp_slot = spsc_acquire(&si7021_temp_spsc);
    *temp_slot = temp_data;
    spsc_produce(&si7021_temp_spsc);

    spsc_reset(&si7021_hume_spec);
    // while (!spsc_acquirable(&si7021_hume_spec)) { k_sleep(K_MSEC(10)); }
    struct sensor_q31_data* hume_slot = spsc_acquire(&si7021_hume_spec);
    *hume_slot = hume_data;
    spsc_produce(&si7021_hume_spec);

    return 0;
}

static void si7021_reader_tfun(void) 
{
    init();

    while (1) {
        k_sem_take(&si7021_readin_start, K_FOREVER);
        sensor_readin();
        k_sem_give(&si7021_readin_finish);
    }
}

K_THREAD_DEFINE(            // Static thread declaration.
    si7021_reader_tid,      // Thread ID.
    1024,                   // Memory (1KB).
    si7021_reader_tfun,     // Entry point.
    NULL, NULL, NULL,       // No parameters.
    1,                      // Priority.
    0,                      // No special options.
    0                       // Start immediately.
);    