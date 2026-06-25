#include <lvgl.h>
#include <zephyr/sys/util.h>

#include "battery.h"
#include "ble/ble.h"
#include "status_bar.h"

#define STATUS_BAR_WIDTH 128
#define STATUS_BAR_HEIGHT 16

static lv_color_t status_bar_buffer[LV_CANVAS_BUF_SIZE_TRUE_COLOR(STATUS_BAR_WIDTH,
								  STATUS_BAR_HEIGHT)];
static lv_obj_t *status_bar;
static lv_obj_t *bt_status_label;

static uint8_t battery_level_from_mv(int32_t mv)
{
	if (mv <= 3300) {
		return 0;
	}
	if (mv >= 4200) {
		return 100;
	}

	return (uint8_t)(((mv - 3300) * 100) / (4200 - 3300));
}

static void status_bar_set_px(int x, int y, lv_color_t color)
{
	if (x < 0 || x >= STATUS_BAR_WIDTH || y < 0 || y >= STATUS_BAR_HEIGHT) {
		return;
	}

	lv_canvas_set_px_color(status_bar, x, y, color);
}

static void status_bar_draw_line(int x0, int y0, int x1, int y1, lv_color_t color)
{
	if (x0 == x1) {
		int y_start = MIN(y0, y1);
		int y_end = MAX(y0, y1);

		for (int y = y_start; y <= y_end; y++) {
			status_bar_set_px(x0, y, color);
		}
		return;
	}

	int x_start = MIN(x0, x1);
	int x_end = MAX(x0, x1);

	for (int x = x_start; x <= x_end; x++) {
		status_bar_set_px(x, y0, color);
	}
}

static void status_bar_draw_rect(int x, int y, int w, int h, lv_color_t color)
{
	status_bar_draw_line(x, y, x + w - 1, y, color);
	status_bar_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
	status_bar_draw_line(x, y, x, y + h - 1, color);
	status_bar_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
}

static void status_bar_fill_rect(int x, int y, int w, int h, lv_color_t color)
{
	for (int yy = y; yy < y + h; yy++) {
		for (int xx = x; xx < x + w; xx++) {
			status_bar_set_px(xx, yy, color);
		}
	}
}

void StatusBar_Refresh()
{
	uint8_t battery_level;
	int fill_width;

	lv_canvas_fill_bg(status_bar, lv_color_white(), LV_OPA_COVER);

	lv_label_set_text(bt_status_label,
			  ble_advertising_is_enabled() ? "BT" : "");

	battery_level = battery_level_from_mv(battery_get_mv());
	status_bar_draw_rect(110, 4, 14, 8, lv_color_black());
	status_bar_fill_rect(124, 6, 2, 4, lv_color_black());
	fill_width = (battery_level * 10) / 100;
	if (fill_width > 0) {
		status_bar_fill_rect(112, 6, fill_width, 4, lv_color_black());
	}

	status_bar_draw_line(0, STATUS_BAR_HEIGHT - 1, STATUS_BAR_WIDTH - 1,
			     STATUS_BAR_HEIGHT - 1, lv_color_black());
	lv_obj_invalidate(status_bar);
}

void StatusBar_Init()
{
	status_bar = lv_canvas_create(lv_layer_top());
	lv_canvas_set_buffer(status_bar, status_bar_buffer, STATUS_BAR_WIDTH,
			     STATUS_BAR_HEIGHT, LV_IMG_CF_TRUE_COLOR);
	lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);

	bt_status_label = lv_label_create(status_bar);
	lv_obj_set_style_text_color(bt_status_label, lv_color_black(), LV_PART_MAIN);
	lv_obj_align(bt_status_label, LV_ALIGN_RIGHT_MID, -22, 0);

	StatusBar_Refresh();
}
