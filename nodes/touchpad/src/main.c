/*
 * M5Stack Core 2 Raw Display Keypad with Bluetooth
 * 6-digit passcode with BLE advertising
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/sys/printk.h>

#define ADV_UPDATE_INTERVAL_MS 1000
#define PASSCODE_LENGTH 6

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define DISPLAY_NODE DT_CHOSEN(zephyr_display)
#define TOUCH_NODE DT_CHOSEN(zephyr_touch)

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define BUTTON_WIDTH  140  // Button width
#define BUTTON_HEIGHT 50   // Button height
#define BUTTON_GAP    10   // Space between buttons

/* Colors (RGB565) */
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_GRAY      0x7BEF
#define COLOR_DARK_GRAY 0x39C7
#define COLOR_GREEN     0x07E0
#define COLOR_RED       0xF800
#define COLOR_BLUE      0x001F

/* Button positions */
typedef struct {
    int x, y, w, h;
    int number;
} button_t;

/* Global variables */
static const struct device *display;
static button_t buttons[5]; /* 4 numbers + 1 enter button */
static char passcode[PASSCODE_LENGTH + 1] = {0};
static int passcode_pos = 0;
static int bluetooth_ready = 0;

/* Draw a filled rectangle */
static void draw_rect(int x, int y, int w, int h, uint16_t color)
{
    static uint16_t line_buf[SCREEN_WIDTH];
    
    /* Fill line buffer with color */
    for (int i = 0; i < w && i < SCREEN_WIDTH; i++) {
        line_buf[i] = color;
    }
    
    struct display_buffer_descriptor desc = {
        .buf_size = w * 2,
        .width = w,
        .height = 1,
        .pitch = w,
    };
    
    /* Draw rectangle line by line */
    for (int row = 0; row < h; row++) {
        display_write(display, x, y + row, &desc, line_buf);
    }
}

/* Simple 5x7 font bitmap for digits */
static const uint8_t font_5x7[][7] = {
    {0x1F, 0x11, 0x11, 0x11, 0x1F}, /* 0 */
    {0x04, 0x0C, 0x04, 0x04, 0x0E}, /* 1 */
    {0x1F, 0x01, 0x1F, 0x10, 0x1F}, /* 2 */
    {0x1F, 0x01, 0x0F, 0x01, 0x1F}, /* 3 */
    {0x11, 0x11, 0x1F, 0x01, 0x01}, /* 4 */
    {0x1F, 0x10, 0x1F, 0x01, 0x1F}, /* 5 */
    {0x1F, 0x10, 0x1F, 0x11, 0x1F}, /* 6 */
    {0x1F, 0x01, 0x01, 0x01, 0x01}, /* 7 */
    {0x1F, 0x11, 0x1F, 0x11, 0x1F}, /* 8 */
    {0x1F, 0x11, 0x1F, 0x01, 0x1F}, /* 9 */
};

/* Draw a character */
static void draw_char(int x, int y, char c, uint16_t color)
{
    if (c < '0' || c > '9') return;
    
    int digit = c - '0';
    
    for (int row = 0; row < 7; row++) {
        uint8_t line = font_5x7[digit][row];
        for (int col = 0; col < 5; col++) {
            if (line & (1 << (4 - col))) {
                /* Draw pixel */
                uint16_t pixel = color;
                struct display_buffer_descriptor desc = {
                    .buf_size = 2,
                    .width = 1,
                    .height = 1,
                    .pitch = 1,
                };
                display_write(display, x + col, y + row, &desc, &pixel);
            }
        }
    }
}

/* Draw text string */
static void draw_text(int x, int y, const char *text, uint16_t color)
{
    int pos_x = x;
    while (*text) {
        if (*text == '*') {
            /* Draw asterisk for hidden passcode */
            draw_rect(pos_x + 2, y + 3, 2, 2, color);
        } else {
            draw_char(pos_x, y, *text, color);
        }
        pos_x += 6; /* 5 pixel width + 1 spacing */
        text++;
    }
}


/* Button positions */
static void init_buttons(void)
{
    /* 2x2 grid for numbers 1-4 */
    for (int i = 0; i < 4; i++) {
        int row = i / 2;  // Determine row (0 or 1)
        int col = i % 2;  // Determine column (0 or 1)
        
        // Shift numbers more to the left
        buttons[i].x = 20 + col * (BUTTON_WIDTH + BUTTON_GAP);  
        buttons[i].y = 80 + row * (BUTTON_HEIGHT + BUTTON_GAP); 
        buttons[i].w = BUTTON_WIDTH;
        buttons[i].h = BUTTON_HEIGHT;
        buttons[i].number = i + 1;
    }
    
    /* Enter button */
    buttons[4].x = 90;
    buttons[4].y = 200;  // Back to original position
    buttons[4].w = BUTTON_WIDTH;
    buttons[4].h = BUTTON_HEIGHT;
    buttons[4].number = -1;  // No number associated with the Enter button
}

/* Draw a button */
static void draw_button(button_t *btn, bool pressed)
{
    uint16_t bg_color = pressed ? COLOR_GRAY : COLOR_DARK_GRAY;
    uint16_t border_color = COLOR_WHITE;
    
    /* Draw button background */
    draw_rect(btn->x, btn->y, btn->w, btn->h, bg_color);
    
    /* Draw border */
    draw_rect(btn->x, btn->y, btn->w, 1, border_color); /* top */
    draw_rect(btn->x, btn->y + btn->h - 1, btn->w, 1, border_color); /* bottom */
    draw_rect(btn->x, btn->y, 1, btn->h, border_color); /* left */
    draw_rect(btn->x + btn->w - 1, btn->y, 1, btn->h, border_color); /* right */
    
    /* Draw button text */
    if (btn->number >= 1 && btn->number <= 4) {
        char text[2] = {'0' + btn->number, '\0'};
        draw_text(btn->x + btn->w / 2 - 3, btn->y + btn->h / 2 - 3, text, COLOR_WHITE);
    } else if (btn->number == 0) {
        draw_text(btn->x + 8, btn->y + btn->h / 2 - 3, "ENTER", COLOR_WHITE);
    }
}

/* Update display with current passcode */
static void update_display(void)
{
    /* Clear display area */
    draw_rect(50, 30, 220, 20, COLOR_BLACK);
    
    /* Show passcode as asterisks if complete, otherwise show digits */
    if (passcode_pos == PASSCODE_LENGTH) {
        draw_text(110, 35, "COMPLETE", COLOR_GREEN);
    } else if (passcode_pos > 0) {
        /* Show asterisks for entered digits */
        char display_text[PASSCODE_LENGTH + 1];
        for (int i = 0; i < passcode_pos; i++) {
            display_text[i] = '*';
        }
        display_text[passcode_pos] = '\0';
        draw_text(55 + (PASSCODE_LENGTH - passcode_pos) * 6, 35, display_text, COLOR_GREEN);
    }
    
    /* Show passcode counter */
    char counter[16];
    snprintf(counter, sizeof(counter), "%d/%d", passcode_pos, PASSCODE_LENGTH);
    draw_text(280, 35, counter, COLOR_BLUE);
}

/* Check if point is inside button */
static int point_in_button(int x, int y)
{
    for (int i = 0; i < 4; i++) { // Check for 4 buttons (1-4)
        if (x >= buttons[i].x && x < buttons[i].x + buttons[i].w &&
            y >= buttons[i].y && y < buttons[i].y + buttons[i].h) {
            return i;
        }
    }
    // Check for the Enter button (button index 4)
    if (x >= buttons[4].x && x < buttons[4].x + buttons[4].w &&
        y >= buttons[4].y && y < buttons[4].y + buttons[4].h) {
        return 4; // Enter button
    }

    return -1;  // Return -1 if no button is pressed
}


/* Handle button press */
static void handle_button_press(int btn_idx)
{
    if (btn_idx < 0 || btn_idx >= 5) return;
    
    button_t *btn = &buttons[btn_idx];
    
    /* Visual feedback */
    draw_button(btn, true);
    k_msleep(100);
    draw_button(btn, false);
    
    if (btn->number >= 1 && btn->number <= 4) {
        /* Number button */
        if (passcode_pos < PASSCODE_LENGTH) {
            passcode[passcode_pos++] = '0' + btn->number;
            passcode[passcode_pos] = '\0';
            LOG_INF("Button %d pressed, passcode position: %d", btn->number, passcode_pos);
            
            if (passcode_pos == PASSCODE_LENGTH) {
                LOG_INF("Passcode complete: %s", passcode);
            }
        }
    } else if (btn->number == 0) {
        /* Enter button */
        if (passcode_pos == PASSCODE_LENGTH) {
            LOG_INF("Passcode entered: %s", passcode);
        }
    }
    
    /* Update display */
    update_display();
}

/* Touch input callback */
static void input_callback(struct input_event *evt, void *user_data)
{
    static int touch_x = -1, touch_y = -1;
    static bool touch_pressed = false;
    
    if (evt->type == INPUT_EV_ABS) {
        if (evt->code == INPUT_ABS_X) {
            touch_x = evt->value;
        } else if (evt->code == INPUT_ABS_Y) {
            touch_y = evt->value;
        }
    } else if (evt->type == INPUT_EV_KEY) {
        if (evt->code == INPUT_BTN_TOUCH) {
            if (evt->value == 1 && !touch_pressed) {
                /* Touch press */
                touch_pressed = true;
                if (touch_x >= 0 && touch_y >= 0) {
                    int btn_idx = point_in_button(touch_x, touch_y);
                    if (btn_idx >= 0) {
                        handle_button_press(btn_idx);
                    }
                }
            } else if (evt->value == 0) {
                /* Touch release */
                touch_pressed = false;
            }
        }
    }
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(TOUCH_NODE), input_callback, NULL);

/* Bluetooth functions */
static void start_advertising_keypad(void)
{
    printk("Starting Bluetooth advertising...\n");

    struct bt_le_adv_param param = {
        .options = BT_LE_ADV_OPT_USE_IDENTITY,
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
        .id = 0,
    };

    struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
        BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA, 0x4C, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06),
    };

    int err = bt_le_adv_start(&param, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Failed to start advertising (err %d)\n", err);
        return;
    }

    printk("Bluetooth advertising started successfully\n");

    while (1) {
        /* Create passcode payload */
        uint8_t passcode_payload[8] = {
            0x4C, 0x00, /* Company ID */
            passcode_pos >= 1 ? passcode[0] : '0',
            passcode_pos >= 2 ? passcode[1] : '0',
            passcode_pos >= 3 ? passcode[2] : '0',
            passcode_pos >= 4 ? passcode[3] : '0',
            passcode_pos >= 5 ? passcode[4] : '0',
            passcode_pos >= 6 ? passcode[5] : '0',
        };

        struct bt_data updated_ad[] = {
            BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
            BT_DATA(BT_DATA_MANUFACTURER_DATA, passcode_payload, sizeof(passcode_payload)),
        };

        err = bt_le_adv_update_data(updated_ad, ARRAY_SIZE(updated_ad), NULL, 0);
        if (err == 0) {
            printk("Advertising updated with passcode: %s (%d digits)\n", 
                   passcode_pos > 0 ? passcode : "empty", passcode_pos);
        }

        k_msleep(ADV_UPDATE_INTERVAL_MS);
    }
}

void bluetooth_thread_keypad(void)
{
    while (!bluetooth_ready) {
        k_msleep(100);
    }
    start_advertising_keypad();
}

void bluetooth_ready_cb(int err)
{
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }
    printk("Bluetooth initialized\n");
    bluetooth_ready = 1;
}

void bluetooth_init_keypad(void)
{
    int err = bt_enable(bluetooth_ready_cb);
    if (err) {
        printk("bt_enable failed (err %d)\n", err);
    }
}

/* Define Bluetooth thread */
K_THREAD_DEFINE(bt_thread, 2048, bluetooth_thread_keypad, NULL, NULL, NULL, 
                K_PRIO_COOP(7), 0, 0);

int main(void)
{
    LOG_INF("M5Stack Keypad with Bluetooth Starting");
    
    /* Initialize display */
    display = DEVICE_DT_GET(DISPLAY_NODE);
    if (!device_is_ready(display)) {
        LOG_ERR("Display not ready");
        return -1;
    }
    
    display_blanking_off(display);
    LOG_INF("Display ready");
    
    /* Initialize Bluetooth */
    bluetooth_init_keypad();
    LOG_INF("Bluetooth initialization started");
    
    /* Clear screen */
    draw_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    
    /* Draw title */
    draw_text(90, 10, "M5STACK PASSCODE", COLOR_WHITE);
    
    /* Draw display area border */
    draw_rect(48, 28, 224, 24, COLOR_DARK_GRAY);
    draw_rect(50, 30, 220, 20, COLOR_BLACK);
    
    /* Initialize and draw buttons */
    init_buttons();
    for (int i = 0; i < 10; i++) {
        draw_button(&buttons[i], false);
    }
    
    /* Initial display update */
    update_display();
    
    LOG_INF("Ready - Touch buttons to enter 6-digit passcode");
    
    /* Main loop */
    while (1) {
        k_msleep(50);
    }
    
    return 0;
}