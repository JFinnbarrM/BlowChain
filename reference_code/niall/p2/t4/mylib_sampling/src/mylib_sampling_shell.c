

#include "mylib_sampling_thread.h"
#include "zephyr/sys/printk.h"

#include <mylib_clock_rtcc.h>

#include <mylib_sensors_bmp280.h>
#include <mylib_sensors_si1133.h>
#include <mylib_sensors_si7021.h>

#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include <string.h>


static void handle_sample_start_did(const struct shell *sh, size_t argc, char **argv) 
{
    int did = atoi(argv[1]);
    sample_did_set(did, true);
}

static void handle_sample_stop_did(const struct shell *sh, size_t argc, char **argv) 
{
    int did = atoi(argv[1]);
    sample_did_set(did, false);
}

static void handle_sample_write_rate(const struct shell *sh, size_t argc, char **argv) 
{
    int freq = atoi(argv[1]);
    if (freq < 1) {
        printk("Sampling frequency can not be less than 1 second\n");
        freq = 1;
    }
    printk("Setting sampling frequency to: %d (seconds)\n", freq);
    sampling_set_frequency(freq);
}

SHELL_STATIC_SUBCMD_SET_CREATE(sample_cmds,
    SHELL_CMD_ARG(s, NULL, "Start sampling a sensor <DID>",  &handle_sample_start_did, 2, 0),
    SHELL_CMD_ARG(p, NULL, "Stop sampling a sensor <DID>", &handle_sample_stop_did, 2, 0),
    SHELL_CMD_ARG(w, NULL, "Write the sample rate <seconds>", &handle_sample_write_rate, 2, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sample, &sample_cmds, "Sample from sensors.", NULL);