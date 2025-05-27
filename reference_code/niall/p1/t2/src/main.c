

#include "zephyr/sys/printk.h"
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>

#include "headers.h"

// Create containers of node information from the node identifiers,
// specified by the device tree:

// - GPIO device
// - Pin number
// - Configuration flags
static const struct gpio_dt_spec rc = GPIO_DT_SPEC_GET_BY_IDX(RC, gpios, 0);
static const struct gpio_dt_spec gc = GPIO_DT_SPEC_GET_BY_IDX(GC, gpios, 0);
static const struct gpio_dt_spec bc = GPIO_DT_SPEC_GET_BY_IDX(BC, gpios, 0);

// - PWM device.
// - Channel number (PWM2 channel 4).
// - Period (nanoseconds).
// - Configuration flags.
static const struct pwm_dt_spec rgb_pwm = PWM_DT_SPEC_GET_BY_IDX(PWM, 0);

// Configure the pins as outputs and set them as active so they can be used.
void configure_pins(void) 
{
    gpio_pin_configure_dt(&rc, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&gc, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&bc, GPIO_OUTPUT_ACTIVE);
}

// Checks and sets the pwm pin to the specified period and pulse.
void set_pwm_pin(void)
{	
     // Adapted from zephyr/samples/basic/blinky_pwm/src/main.c
    if (!pwm_is_ready_dt(&rgb_pwm)) {
    printk("Error: PWM DEVICE %s FAILED\n", rgb_pwm.dev->name);
    } else {
        pwm_set_dt(&rgb_pwm, 1000 , 990);
    }
}

// Sets the RGB channel states based on the desired colour.
void set_colour_to(rgb_colour colour) {
    rgb_state combo = {ON, ON, ON};
    switch (colour) {
        case BLACK:     combo = (rgb_state){ OFF, OFF, OFF }; break;
        case BLUE:      combo = (rgb_state){ OFF, OFF, ON  }; break;
        case GREEN:     combo = (rgb_state){ OFF, ON,  OFF }; break;
        case CYAN:      combo = (rgb_state){ OFF, ON,  ON  }; break;
        case RED:       combo = (rgb_state){ ON,  OFF, OFF }; break;
        case MAGENTA:   combo = (rgb_state){ ON,  OFF, ON  }; break;
        case YELLOW:    combo = (rgb_state){ ON,  ON,  OFF }; break;
        case WHITE:     combo = (rgb_state){ ON,  ON,  ON  }; break;
        default: printk("colour not recognised"); break;
    }
    gpio_pin_set_dt(&rc, combo.red);
    gpio_pin_set_dt(&gc, combo.green);
    gpio_pin_set_dt(&bc, combo.blue);
}

// Allows communication of an integer between threads that can be translated into a colour.
K_MSGQ_DEFINE(          
    rgb_msgq,           // Name.
    sizeof(int),        // Message size.
    1,                  // Maximum messages.
    1                   // Memory alignment.                   
);                      

// Create the static RGB LED thread at compile time.
K_THREAD_DEFINE(            // Static thread declaration.
    rgb_led_tid,            // Thread ID.
    1024,                   // Memory (1KB).
    rgb_led_tfun,           // Entry point.
    NULL, NULL, NULL,       // No parameters.
    1,                      // HIGHER priority.
    0,                      // No special options.
    2000                    // Start after 2 seconds.
);    

// Create the static control thread at compile time.
K_THREAD_DEFINE(            // Static thread declaration.
    control_tid,            // Thread ID.
    1024,                   // Memory (1KB).
    control_tfun,           // Entry point.
    NULL, NULL, NULL,       // No parameters.
    1,                      // LOWER priority.
    0,                      // No special options.
    2000                    // Start after 2 seconds.
);    

// Decides on the colour by looping through the rgb_colour values and putting them
// into the message queue.
void rgb_led_tfun(void) 
{
    rgb_colour colour = BLACK;

    while (true)
    {
        int result = k_msgq_put 
        (
            &rgb_msgq,
            &colour,
            K_MSEC(100)
        );

        if (result != 0) {
            printk("RGB LED THREAD FAILED");
        }

        colour += 1;            // Incriment through the colours.
        colour %= NUM_COLOURS;  // Loop back to the start of the sequence.

        k_sleep(K_MSEC(2000));
    }
}

// Sets the desired colour by getting from the message queue.
void control_tfun(void)
{
    while (true)
    {
        rgb_colour colour;

        int result = k_msgq_get 
        (
            &rgb_msgq,     
            &colour, 
            K_MSEC(100) 
        );

        if (result != 0) {
            printk("CONTROL THREAD FAILED");
        } 
        else {
            set_colour_to(colour); 
        }

        k_sleep(K_MSEC(2000));
    }
}

// Entry point of program, 
// Calls for pin configuration, sets the PWM pin, and labels static threads before returning.
int main(void) {

    configure_pins();
    set_pwm_pin();

    k_thread_name_set(rgb_led_tid, "RGB-LED-Thread");
    k_thread_name_set(control_tid, "Control-Thread");

    printk("\n\nITS RAINBOWING TIME !!!\n\n");

    return 0;
}