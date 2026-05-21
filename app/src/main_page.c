#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/sensor.h>

#include <math.h>
#include <stdio.h>

#include "battery.h"
#include "gnss.h"

#define LVGL_STACK_SIZE 2048
#define LVGL_PRIORITY 5
K_THREAD_STACK_DEFINE(lvgl_stack_area, LVGL_STACK_SIZE);
struct k_thread lvgl_thread_data;
k_tid_t lvgl_tid;
struct k_mutex lvgl_mutex;
extern struct device* display_dev;
extern struct device* pressure_dev;
struct lv_objs{
		lv_obj_t* bat;
		lv_obj_t* prs;
		lv_obj_t* tmp;
		lv_obj_t* alt;
		lv_obj_t* gnss;
};

static uint32_t cnt = 0;

static void format_coord(int32_t coord_e6, bool is_latitude, char *buf, size_t buf_size)
{
	char hemi;
	int32_t abs_coord;
	int32_t degrees;
	int32_t fraction;

	hemi = is_latitude ? 'N' : 'E';
	abs_coord = coord_e6;

	if (coord_e6 < 0) {
		hemi = is_latitude ? 'S' : 'W';
		abs_coord = -coord_e6;
	}

	degrees = abs_coord / 1000000;
	fraction = abs_coord % 1000000;
	snprintf(buf, buf_size, "%ld.%06ld%c",
		 (long)degrees, (long)fraction, hemi);
}

static const char *hdop_grade(uint16_t hdop_x10)
{
	if (hdop_x10 == 0U) {
		return "--";
	}
	if (hdop_x10 < 10U) {
		return "Excellent";
	}
	if (hdop_x10 < 20U) {
		return "Good";
	}
	if (hdop_x10 < 50U) {
		return "Fair";
	}
	return "Poor";
}

void info_update(lv_timer_t * timer)
{
	struct lv_objs* objs = timer->user_data;
	struct gnss_info gnss;
	int32_t bat_level = battery_get_mv();
	struct sensor_value pressure;
	struct sensor_value temperature;
	int altitude;
	uint8_t bat_buf[8];
	uint8_t prs_buf[20];
	uint8_t tmp_buf[18];
	uint8_t alt_buf[20];
	char lat_buf[20];
	char lon_buf[20];
	const char *hdop_text;
	char gnss_buf[128];
	sprintf(bat_buf, "%4dmv", bat_level);
	sensor_sample_fetch(pressure_dev);
	sensor_channel_get(pressure_dev, SENSOR_CHAN_PRESS, &pressure);
	sensor_channel_get(pressure_dev, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
	altitude = 443300 * (1 - pow((pressure.val1+pressure.val1/1000000.0)/101325.0, 1/5.255));
	
	sprintf(prs_buf, "%6d.%01dPa", pressure.val1, pressure.val2/100000);
	sprintf(tmp_buf, "%2d.%02dC", temperature.val1, temperature.val2/10000);
	sprintf(alt_buf, "%4d.%01dm", altitude/10, altitude%10);
	gnss_get_info(&gnss);
	hdop_text = hdop_grade(gnss.hdop_x10);

	if (gnss.has_fix) {
		format_coord(gnss.lat_e6, true, lat_buf, sizeof(lat_buf));
		format_coord(gnss.lon_e6, false, lon_buf, sizeof(lon_buf));
		snprintf(gnss_buf, sizeof(gnss_buf),
			 "Lat:%s\nLon:%s\nHDOP:%u.%u %s\nQ:%u\nSat:%u\nSpd:%u.%ukm/h\nCog:%u.%udeg",
			 lat_buf, lon_buf,
			 gnss.hdop_x10 / 10U, gnss.hdop_x10 % 10U, hdop_text,
 			gnss.quality,
			 gnss.satellites,
			 gnss.speed_kmh_x10 / 10U, gnss.speed_kmh_x10 % 10U,
			 gnss.course_deg_x10 / 10U, gnss.course_deg_x10 % 10U);
	} else {
		snprintf(gnss_buf, sizeof(gnss_buf),
			 "Lat:--\nLon:--\nHDOP:%u.%u %s\nQ:%u\nSat:%u\nSpd:%u.%ukm/h\nCog:%u.%udeg",
			 gnss.hdop_x10 / 10U, gnss.hdop_x10 % 10U, hdop_text,
			gnss.quality, 
			 gnss.satellites,
			 gnss.speed_kmh_x10 / 10U, gnss.speed_kmh_x10 % 10U,
			 gnss.course_deg_x10 / 10U, gnss.course_deg_x10 % 10U);
	}

	k_mutex_lock(&lvgl_mutex, K_FOREVER);
	lv_label_set_text(objs->bat, bat_buf);
	lv_label_set_text(objs->prs, prs_buf);
	lv_label_set_text(objs->tmp, tmp_buf);
	lv_label_set_text(objs->alt, alt_buf);
	lv_label_set_text(objs->gnss, gnss_buf);

	k_mutex_unlock(&lvgl_mutex);
}

void lvgl_entry_point(void *, void *, void *)
{
	lv_obj_t * bat_label;
	lv_obj_t * prs_label;
	lv_obj_t * tmp_label;
	lv_obj_t * alt_label;
	lv_obj_t * gnss_label;
	lv_timer_t * info_update_timer;
	struct lv_objs main_page_objs;

	k_mutex_lock(&lvgl_mutex, K_FOREVER);
	bat_label = lv_label_create(lv_scr_act());
	lv_label_set_text(bat_label, "Bat:");
	lv_obj_align(bat_label, LV_ALIGN_TOP_LEFT, 0, 20);
	prs_label = lv_label_create(lv_scr_act());
	lv_label_set_text(prs_label, "Prs:");
	lv_obj_align(prs_label, LV_ALIGN_TOP_LEFT, 0, 36);
	tmp_label = lv_label_create(lv_scr_act());
	lv_label_set_text(tmp_label, "Temp:");
	lv_obj_align(tmp_label, LV_ALIGN_TOP_LEFT, 0, 52);
	alt_label = lv_label_create(lv_scr_act());
	lv_label_set_text(alt_label, "Altitude:");
	lv_obj_align(alt_label, LV_ALIGN_TOP_LEFT, 0, 68);
	gnss_label = lv_label_create(lv_scr_act());
	lv_label_set_text(gnss_label, "GNSS:");
	lv_obj_align(gnss_label, LV_ALIGN_TOP_LEFT, 0, 84);

	main_page_objs.bat = lv_label_create(lv_scr_act());
	lv_label_set_text(main_page_objs.bat, "0000mv");
	lv_obj_align(main_page_objs.bat, LV_ALIGN_TOP_LEFT, 32, 20);
	main_page_objs.prs = lv_label_create(lv_scr_act());
	lv_label_set_text(main_page_objs.prs, "000000Pa");
	lv_obj_align(main_page_objs.prs, LV_ALIGN_TOP_LEFT, 32, 36);
	main_page_objs.tmp = lv_label_create(lv_scr_act());
	lv_label_set_text(main_page_objs.tmp, "00C");
	lv_obj_align(main_page_objs.tmp, LV_ALIGN_TOP_LEFT, 40, 52);
	main_page_objs.alt = lv_label_create(lv_scr_act());
	lv_label_set_text(main_page_objs.alt, "0000m");
	lv_obj_align(main_page_objs.alt, LV_ALIGN_TOP_LEFT, 72, 68);
	main_page_objs.gnss = lv_label_create(lv_scr_act());
	lv_label_set_long_mode(main_page_objs.gnss, LV_LABEL_LONG_WRAP);
	lv_obj_set_width(main_page_objs.gnss, 128);
	lv_label_set_text(main_page_objs.gnss, "Lat:--\nLon:--\nHDOP:0.0 --\nQ:0\nSat:0\nSpd:0.0km/h\nCog:0.0deg");
	lv_obj_align(main_page_objs.gnss, LV_ALIGN_TOP_LEFT, 0, 94);

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
    k_mutex_init(&lvgl_mutex);
	lvgl_tid = k_thread_create(&lvgl_thread_data, lvgl_stack_area,
                                 K_THREAD_STACK_SIZEOF(lvgl_stack_area),
                                 lvgl_entry_point,
                                 NULL, NULL, NULL,
                                 LVGL_PRIORITY, 0, K_NO_WAIT);
	display_blanking_off(display_dev);
}
