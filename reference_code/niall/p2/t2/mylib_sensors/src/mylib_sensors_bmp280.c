// Inspired by zephyr/samples/sensor/bme280

#include "mylib_sensors_bmp280.h"
#include "zephyr/drivers/sensor_data_types.h"
#include "zephyr/shell/shell.h"

#include <mylib_shell_globalsh.h>

#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/spsc_lockfree.h>

LOG_MODULE_REGISTER(bmp280_thread);

static const struct device* dev = DEVICE_DT_GET(DT_NODELABEL(bmp280));
SENSOR_DT_READ_IODEV( // Sets channels for sensor type.
                      // So the reader and decoders know what they're looking at.

    bpm280_iodev, 
    DT_NODELABEL(bmp280),
	{SENSOR_CHAN_AMBIENT_TEMP, 0},
	{SENSOR_CHAN_PRESS, 0}
);
RTIO_DEFINE(bmp280_ctx, 1, 1); // https://docs.zephyrproject.org/latest/services/rtio/index.html

// SPSC used as a message storing/passing primitive as required by spec.
SPSC_DEFINE(bmp280_temp_spsc, struct sensor_q31_data, 2);
SPSC_DEFINE(bmp280_pres_spsc, struct sensor_q31_data, 2);
K_SEM_DEFINE(bmp280_readin_start, 0, 1);
K_SEM_DEFINE(bmp280_readin_finish, 0, 1);


int bmp280_get_temp_data(struct sensor_q31_data *data) {
    k_sem_give(&bmp280_readin_start);
    k_sem_take(&bmp280_readin_finish, K_FOREVER);

    if (!spsc_consumable(&bmp280_temp_spsc)) return -1;
    *data = *spsc_consume(&bmp280_temp_spsc);
    spsc_release(&bmp280_temp_spsc);
    return 0;
}

int bmp280_get_pres_data(struct sensor_q31_data *data) {
    k_sem_give(&bmp280_readin_start);
    k_sem_take(&bmp280_readin_finish, K_FOREVER);

    if (!spsc_consumable(&bmp280_pres_spsc)) return -1;
    *data = *spsc_consume(&bmp280_pres_spsc);
    spsc_release(&bmp280_pres_spsc);
    return 0;
}

void bmp280_get_tempstamp(char* tempstamp, struct sensor_q31_data* temp_data)
{
    snprintf(tempstamp, 32,
        "%" PRIq(2) "",
        PRIq_arg(temp_data->readings[0].pressure, 2, temp_data->shift)
    );
}

void bmp280_printk_temp(struct sensor_q31_data* temp_data) 
{
    char tempstamp[32];
    bmp280_get_tempstamp(tempstamp, temp_data);
    printk("(bpm280) temperature=\t%s (C)\n", tempstamp);
}

void bmp280_printk_temp_shell(struct sensor_q31_data* temp_data) 
{
    char tempstamp[32];
    bmp280_get_tempstamp(tempstamp, temp_data);
    shell_print(sh_global, "(bpm280) temperature=\t%s (C)\n", tempstamp);
}

void bmp280_get_presstamp(char* presstamp, struct sensor_q31_data* pres_data)
{
    snprintf(presstamp, 32,
        "%" PRIq(2) "",
        PRIq_arg(pres_data->readings[0].pressure, 2, pres_data->shift)
    );
}

void bmp280_printk_pres(struct sensor_q31_data* pres_data) 
{
    char presstamp[32];
    bmp280_get_presstamp(presstamp, pres_data);
    printk("(bpm280) pressure=\t%s (kPa)\n", presstamp);
}

void bmp280_printk_pres_shell(struct sensor_q31_data* pres_data) 
{
    char presstamp[32];
    bmp280_get_presstamp(presstamp, pres_data);
    shell_print(sh_global, "(bpm280) pressure=\t%s (kPa)\n", presstamp);
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
    int rc = sensor_read(&bpm280_iodev, &bmp280_ctx, buf, 128);
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

    uint32_t pres_fit = 0;
    struct sensor_q31_data press_data = {0};
    decoder->decode(buf,
            (struct sensor_chan_spec) {SENSOR_CHAN_PRESS, 0},
            &pres_fit, 1, &press_data);

    spsc_reset(&bmp280_temp_spsc);
    // while (!spsc_acquirable(&bmp280_temp_spsc)) { k_sleep(K_MSEC(10)); }
    struct sensor_q31_data* temp_slot = spsc_acquire(&bmp280_temp_spsc);
    *temp_slot = temp_data;
    spsc_produce(&bmp280_temp_spsc);

    spsc_reset(&bmp280_pres_spsc);
    // while (!spsc_acquirable(&bmp280_pres_spsc)) { k_sleep(K_MSEC(10)); }
    struct sensor_q31_data* pres_slot = spsc_acquire(&bmp280_pres_spsc);
    *pres_slot = press_data;
    spsc_produce(&bmp280_pres_spsc);

    return 0;
}

static void bmp280_reader_tfun(void) 
{
    init();

    while (1) {
        k_sem_take(&bmp280_readin_start, K_FOREVER);
        sensor_readin();
        k_sem_give(&bmp280_readin_finish);
    }
}

K_THREAD_DEFINE(            // Static thread declaration.
    bmp280_reader_tid,      // Thread ID.
    1024,                   // Memory (1KB).
    bmp280_reader_tfun,     // Entry point.
    NULL, NULL, NULL,       // No parameters.
    1,                      // Priority.
    0,                      // No special options.
    0                       // Start immediately.
);    