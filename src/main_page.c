#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/display.h>

#include "battery.h"

#define LVGL_STACK_SIZE 2048
#define LVGL_PRIORITY 5
K_THREAD_STACK_DEFINE(lvgl_stack_area, LVGL_STACK_SIZE);
struct k_thread lvgl_thread_data;
k_tid_t lvgl_tid;
struct k_mutex lvgl_mutex;
const struct device *display_dev;
struct lv_objs{
		lv_obj_t* bat;
};

static uint32_t cnt = 0;
void info_update(lv_timer_t * timer)
{
	struct lv_objs* objs = timer->user_data;
	int32_t bat_level = battery_get_mv();
	uint8_t bat_buf[8];
	sprintf(bat_buf, "%4dmv", bat_level);
	k_mutex_lock(&lvgl_mutex, K_FOREVER);
	lv_label_set_text(objs->bat, bat_buf);

	k_mutex_unlock(&lvgl_mutex);
}

void lvgl_entry_point(void *, void *, void *)
{
	lv_obj_t * bat_label;
	lv_timer_t * info_update_timer;
	struct lv_objs main_page_objs;

	k_mutex_lock(&lvgl_mutex, K_FOREVER);
	bat_label = lv_label_create(lv_scr_act());
	lv_label_set_text(bat_label, "Bat:");
	lv_obj_align(bat_label, LV_ALIGN_TOP_LEFT, 0, 20);
	main_page_objs.bat = lv_label_create(lv_scr_act());
	lv_label_set_text(main_page_objs.bat, "0000mv");
	lv_obj_align(main_page_objs.bat, LV_ALIGN_TOP_LEFT, 32, 20);

	/*Create a buffer for the canvas*/
    #define CANVAS_WIDTH  128
	#define CANVAS_HEIGHT  16
	static lv_color_t cbuf[LV_CANVAS_BUF_SIZE_TRUE_COLOR(CANVAS_WIDTH, CANVAS_HEIGHT)];
    lv_obj_t * canvas = lv_canvas_create(lv_scr_act());
    lv_canvas_set_buffer(canvas, cbuf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(canvas, LV_ALIGN_TOP_MID, 0, 0);
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);
	info_update_timer = lv_timer_create(info_update, 500, &main_page_objs);
	lv_timer_set_repeat_count(info_update_timer, -1);
	lv_timer_ready(info_update_timer);
	lv_task_handler();
	k_mutex_unlock(&lvgl_mutex);
	for(;;)
	{
		k_mutex_lock(&lvgl_mutex, K_FOREVER);
		lv_task_handler();
		lv_timer_handler();
        k_mutex_unlock(&lvgl_mutex);
		k_msleep(5);
	}
}

void main_page()
{
	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    k_mutex_init(&lvgl_mutex);
	lvgl_tid = k_thread_create(&lvgl_thread_data, lvgl_stack_area,
                                 K_THREAD_STACK_SIZEOF(lvgl_stack_area),
                                 lvgl_entry_point,
                                 NULL, NULL, NULL,
                                 LVGL_PRIORITY, 0, K_NO_WAIT);
	display_blanking_off(display_dev);
}