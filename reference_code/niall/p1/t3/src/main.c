
#include "header.h"
#include "stm32l4xx_hal_def.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(led_module);

// The handler function for a shell command that displays system uptime.
static int time_cmd_handler(const struct shell *sh, size_t argc, char **argv)
{
    // Get the system uptime in seconds.
    int64_t uptime_ms = k_uptime_get();
    int uptime_sec = uptime_ms / 1000;

    // Behavior if there was no parameters.
    if (argc == 1) {
        shell_print(sh, "UPTIME: %u (seconds))", uptime_sec);
        return 0;
    };

    // Behavior with an incorrect parameter (not "f").
    if (strcmp(argv[1],"f") != 0) {
        LOG_ERR("'%s' is an invalid argument for the 'time' command", argv[1]);
        return -1;
    };

    // Behavior with the correct parameter ("f").
    // Format the time as 'HH:MM:SS'.
    int hours = uptime_sec / 3600;
    int minutes = (uptime_sec % 3600) / 60;
    int seconds = uptime_sec % 60;
    shell_print(sh, "UPTIME: %02dh:%02dm:%02ds", hours, minutes, seconds);
    return 0;
}

SHELL_CMD_ARG_REGISTER( // Create a root command that can take arguments.
    time,               // Type 'time' in shell to use command.
    NULL,               // No subcommand array.
    "Show system uptime (use 'f' for formatted output)", // help info.
    time_cmd_handler,   // Function that handles execution of the command.
    1,                  // Mandatory arguments (including command name).
    1                   // Optional arguments.
);

// The handler function for a shell command that sets and toggles two LEDs states.
static int led_cmd_handler(const struct shell *sh, size_t argc, char **argv)
{
    // Check which action was selected.
    bool set_action = (strcmp(argv[1], "s") == 0);
    bool tog_action = (strcmp(argv[1], "t") == 0);
    if (!set_action && !tog_action) { LOG_WRN("invalid command: '%s' is an invalid 1st argument", argv[1]); };

    // Check that the second argument is "00", "01", "10", or "11".
    if (
        strcmp(argv[2], "00") != 0 &&
        strcmp(argv[2], "01") != 0 &&
        strcmp(argv[2], "10") != 0 &&
        strcmp(argv[2], "11") != 0
    ) {
        LOG_ERR("invalid command: '%s' is an invalid 2nd argument", argv[2]);
        return -1; //Exit the command if it isnt.
    }

    // Get each input bit out of the string using ascii subtraction.
    bool bit1on = argv[2][1] - '0';
    bool bit2on = argv[2][0] - '0';

    if (set_action) { // Sets the leds while logging active states.

        if (bit1on) { // See if led1 is requested to be set.
            bool led1on = gpio_pin_get_dt(&led1);
            if (led1on) {
                LOG_WRN("led 1 already on");
            } else { // If its not already on, set it.
                gpio_pin_set_dt(&led1, 1);
                LOG_INF("led 1 set on");
            };
        } else {
            gpio_pin_set_dt(&led1, 0);
        }

        if (bit2on) { // See if led2 is requested to be set.
            bool led2on = gpio_pin_get_dt(&led2);
            if (led2on) {
                LOG_WRN("led 2 already on");
            } else { // If its not already on, set it.
                gpio_pin_set_dt(&led2, 1);
                LOG_INF("led 2 set on");
            };
        } else {
            gpio_pin_set_dt(&led2, 0);
        }

        return 0; // Finished.
    };
    
    if (tog_action) { //Toggles the leds while logging active states.

        if (bit1on) { // See if led1 needs to be toggled,
            gpio_pin_toggle_dt(&led1); 
            if (gpio_pin_get_dt(&led1)) { // Log when it toggles on.
                LOG_INF("led 1 toggled on");
            };
        }

        if (bit2on) { // See if led2 needs to be toggled.
            gpio_pin_toggle_dt(&led2); 
            if (gpio_pin_get_dt(&led2)) { // Log when it toggles on.
                LOG_INF("led 2 toggled on");
            };
        }

        return 0; // Finished.
    };

    return -1; // If the 1st argument wasnt "s" or "t".
}

SHELL_CMD_ARG_REGISTER( // Create a root command that can take arguments.
    led,                // Type 'led' in shell to use command.
    NULL,               // No subcommand array.
    "Control LEDs <s: set, t: toggle> <00 or 01 or 10 or 11>", // Help info.
    led_cmd_handler,    // Function that handles execution of the command.
    3,                  // Mandatory arguments (including command name).
    0                   // Optional arguments.
);

int main(void)
{
    int result;

    result = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
    // Log the results of attempting to configure led1.
    if (result != 0) { LOG_ERR("Failed to configure led 1 (red): %s", strerror(-result)); }
    else { LOG_INF("led 1 (blue) configured as an output"); }

    result = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
    // Log the results of attempting to configure led2.
    if (result != 0) { LOG_ERR("Failed to configure led 2 (green): %s", strerror(-result)); }
    else { LOG_INF("led 2 (red) configured as an output"); }

    LOG_INF("started successfully");

    return 0;
}