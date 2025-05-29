#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/display.h>
#include <lvgl.h>
#include <sys/printk.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(main);

#define LV_TICK_PERIOD_MS 5

static const struct device *display_dev;

static void lv_tick_handler(struct k_timer *timer_id);

K_TIMER_DEFINE(lv_tick_timer, lv_tick_handler, NULL);

static void lv_tick_handler(struct k_timer *timer_id)
{
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

static void create_keypad(void)
{
    lv_obj_t *btn;
    lv_obj_t *label;

    const char *keys[] = {
        "1", "2", "3",
        "4", "5", "6",
        "7", "8", "9",
        "*", "0", "#"
    };

    lv_obj_t *cont = lv_cont_create(lv_scr_act());
    lv_obj_set_size(cont, 230, 320);
    lv_obj_center(cont);
    lv_cont_set_layout(cont, LV_LAYOUT_GRID);
    lv_cont_set_fit(cont, LV_FIT_TIGHT);

    static const lv_coord_t col_dsc[] = {70, 70, 70, LV_GRID_TEMPLATE_LAST};
    static const lv_coord_t row_dsc[] = {70, 70, 70, 70, LV_GRID_TEMPLATE_LAST};

    lv_obj_set_grid_dsc_array(cont, col_dsc, row_dsc);

    for (int i = 0; i < 12; i++) {
        btn = lv_btn_create(cont);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_CENTER, i % 3, 1,
                                    LV_GRID_ALIGN_CENTER, i / 3, 1);
        label = lv_label_create(btn);
        lv_label_set_text(label, keys[i]);
        lv_obj_center(label);

        // Optional: Add event handler to buttons if needed
    }
}

void main(void)
{
    printk("Starting M5Stack Core2 keypad demo\n");

    display_dev = device_get_binding("DISPLAY");
    if (!display_dev) {
        printk("Display device not found\n");
        return;
    }

    // Initialize LVGL
    lv_init();

    // Initialize display driver buffer
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[LV_HOR_RES_MAX * 10];
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, LV_HOR_RES_MAX * 10);

    // Setup display driver for LVGL
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = display_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.hor_res = 320;
    disp_drv.ver_res = 240;
    lv_disp_drv_register(&disp_drv);

    // Start LVGL tick timer
    k_timer_start(&lv_tick_timer, K_MSEC(LV_TICK_PERIOD_MS), K_MSEC(LV_TICK_PERIOD_MS));

    // Create keypad GUI
    create_keypad();

    while (1) {
        lv_task_handler();
        k_sleep(K_MSEC(10));
    }
}

/**
 * Display flush callback required by LVGL.
 * This will push LVGL's framebuffer to the actual display.
 */
void display_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    struct display_buffer_descriptor desc;
    int ret;

    desc.buf_size = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) * sizeof(lv_color_t);
    desc.width = area->x2 - area->x1 + 1;
    desc.height = area->y2 - area->y1 + 1;
    desc.pitch = desc.width;

    ret = display_write(display_dev, area->x1, area->y1, color_p, desc.buf_size);
    if (ret < 0) {
        printk("Display write error: %d\n", ret);
    }

    lv_disp_flush_ready(disp_drv);
}
