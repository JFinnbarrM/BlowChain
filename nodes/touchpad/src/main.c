#ifdef CONFIG_INPUT_KBD_MATRIX
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/shell/shell.h>
#include <zephyr/input/input_kbd_matrix.h>
#include <zephyr/dt-bindings/input/keymap.h>
#include <zephyr/input/input_keymap.h>

#define STACK_SIZE 1024
#define THREAD_PRIORITY 7

typedef struct {
    size_t x;
    size_t y;
    bool pressed;
} KeypadEvent;

KeypadEvent keypad;

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(keypad, LOG_LEVEL_DBG);

static const struct device *const kp_dev = DEVICE_DT_GET(DT_NODELABEL(kbdmatrix));

void (*user_kp_callback)(char letter) = NULL;

char keys[3][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'}
};

static void kp_callback(struct input_event *evt, void *user_data)
{  
    char letter = 'K';
    if (evt->code == INPUT_ABS_X) {
        keypad.x = evt->value;
    }
    if (evt->code == INPUT_ABS_Y) {
        keypad.y = evt->value;
    }
    if (evt->code == INPUT_BTN_TOUCH) {
        keypad.pressed = evt->value;
    }
    if (!evt->sync) {
        return;
    }

    if (keypad.pressed && keypad.x < 3 && keypad.y < 3) {
        LOG_DBG("Keypad event: x=%zu, y=%zu, pressed=%d", keypad.x, keypad.y, keypad.pressed);
        letter = keys[keypad.y][keypad.x];
        LOG_DBG("Key pressed: %c", letter);

        keypad.x = 50;
        keypad.y = 50;

        if (user_kp_callback) {
            user_kp_callback(letter);
        }
    }
}

// This macro registers the callback automatically.
INPUT_CALLBACK_DEFINE(kp_dev, kp_callback, NULL);

#endif // CONFIG_INPUT_KBD_MATRIX

void set_user_kp_callback(void (*callback)(char letter)) {
    #ifdef CONFIG_INPUT_GPIO_KBD_MATRIX
        user_kp_callback = callback;
    #else
        LOG_WRN("No keypad available");
    #endif
}
