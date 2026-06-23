#include <lvgl.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/util.h>

#include <math.h>
#include <stdio.h>

#include "battery.h"
#include "gnss.h"
#include "info_page.h"

extern const struct device *pressure_dev;

struct info_page_widgets {
	lv_obj_t *bat;
	lv_obj_t *prs;
	lv_obj_t *tmp;
	lv_obj_t *alt;
	lv_obj_t *gnss;
};

static struct info_page_widgets info_widgets;
static lv_timer_t *refresh_timer;

static const char *hdop_quality_text(uint16_t hdop_x10)
{
	if (hdop_x10 == 0U) {
		return "--";
	}
	if (hdop_x10 < 10U) {
		return "Exce";
	}
	if (hdop_x10 < 20U) {
		return "Good";
	}
	if (hdop_x10 < 50U) {
		return "Fair";
	}
	return "Poor";
}

static void refresh_info(lv_timer_t *timer)
{
	struct info_page_widgets *widgets =
		static_cast<struct info_page_widgets *>(timer->user_data);
	struct gnss_data gnss;
	bool has_fix;
	int32_t bat_level = battery_get_mv();
	struct sensor_value sensor_pressure;
	struct sensor_value sensor_temperature;
	int altitude;
	char battery_text[8];
	char pressure_text[20];
	char temperature_text[18];
	char altitude_text[20];
	const char *hdop_text;
	char gnss_text[160];
	uint16_t hdop_x10 = 0U;
	uint16_t speed_kmh_x10 = 0U;
	uint16_t course_deg_x10 = 0U;
	uint64_t latitude = 0ULL;
	uint64_t longitude = 0ULL;

	sprintf(battery_text, "%4dmv", bat_level);
	sensor_sample_fetch(pressure_dev);
	sensor_channel_get(pressure_dev, SENSOR_CHAN_PRESS, &sensor_pressure);
	sensor_channel_get(pressure_dev, SENSOR_CHAN_AMBIENT_TEMP, &sensor_temperature);
	altitude = 443300 * (1 - pow((sensor_pressure.val1 + sensor_pressure.val1 / 1000000.0) / 101325.0,
				      1 / 5.255));

	sprintf(pressure_text, "%6d.%01dPa", sensor_pressure.val1, sensor_pressure.val2 / 100000);
	sprintf(temperature_text, "%2d.%02dC", sensor_temperature.val1,
		sensor_temperature.val2 / 10000);
	sprintf(altitude_text, "%4d.%01dm", altitude / 10, altitude % 10);
	has_fix = gnss_get_info(&gnss) &&
		  gnss.info.fix_status != GNSS_FIX_STATUS_NO_FIX;
	if (has_fix) {
		hdop_x10 = (uint16_t)(gnss.info.hdop / 100U);
		speed_kmh_x10 = (uint16_t)((gnss.nav_data.speed * 36ULL) / 1000ULL);
		course_deg_x10 = (uint16_t)(gnss.nav_data.bearing / 100U);
		latitude = gnss.nav_data.latitude < 0 ? -gnss.nav_data.latitude :
			   gnss.nav_data.latitude;
		longitude = gnss.nav_data.longitude < 0 ? -gnss.nav_data.longitude :
			    gnss.nav_data.longitude;
	}
	hdop_text = hdop_quality_text(hdop_x10);

	if (has_fix) {
		snprintf(gnss_text, sizeof(gnss_text),
			 "Lat:%llu.%06llu%s\nLon:%llu.%06llu%s\nHDOP:%u.%u %s\nQ:%u\nSat:%u\nSpd:%u.%u km/h\nCog:%u.%u deg",
			 latitude / 1000000000ULL,
			 (latitude % 1000000000ULL) / 1000ULL,
			 gnss.nav_data.latitude < 0 ? "S" : "N",
			 longitude / 1000000000ULL,
			 (longitude % 1000000000ULL) / 1000ULL,
			 gnss.nav_data.longitude < 0 ? "W" : "E",
			 hdop_x10 / 10U, hdop_x10 % 10U, hdop_text,
			 gnss.info.fix_quality,
			 gnss.info.satellites_cnt,
			 speed_kmh_x10 / 10U, speed_kmh_x10 % 10U,
			 course_deg_x10 / 10U, course_deg_x10 % 10U);
	} else {
		snprintf(gnss_text, sizeof(gnss_text),
			 "Lat:--\nLon:--\nHDOP:%u.%u %s\nQ:%u\nSat:%u\nSpd:%u.%u km/h\nCog:%u.%u deg",
			 hdop_x10 / 10U, hdop_x10 % 10U, hdop_text,
			 0U, 0U, 0U, 0U, 0U, 0U);
	}

	lv_label_set_text(widgets->bat, battery_text);
	lv_label_set_text(widgets->prs, pressure_text);
	lv_label_set_text(widgets->tmp, temperature_text);
	lv_label_set_text(widgets->alt, altitude_text);
	lv_label_set_text(widgets->gnss, gnss_text);
}

static void setup_info_page(int arg)
{
	lv_obj_t *bat_label;
	lv_obj_t *prs_label;
	lv_obj_t *tmp_label;
	lv_obj_t *alt_label;
	lv_obj_t *gnss_label;

	ARG_UNUSED(arg);

	bat_label = lv_label_create(lv_scr_act());
	lv_label_set_text(bat_label, "Bat:");
	lv_obj_align(bat_label, LV_ALIGN_TOP_LEFT, 0, 16);
	prs_label = lv_label_create(lv_scr_act());
	lv_label_set_text(prs_label, "Prs:");
	lv_obj_align(prs_label, LV_ALIGN_TOP_LEFT, 0, 26);
	tmp_label = lv_label_create(lv_scr_act());
	lv_label_set_text(tmp_label, "Temp:");
	lv_obj_align(tmp_label, LV_ALIGN_TOP_LEFT, 0, 36);
	alt_label = lv_label_create(lv_scr_act());
	lv_label_set_text(alt_label, "Altitude:");
	lv_obj_align(alt_label, LV_ALIGN_TOP_LEFT, 0, 46);
	gnss_label = lv_label_create(lv_scr_act());
	lv_label_set_text(gnss_label, "GNSS(WGS84):");
	lv_obj_align(gnss_label, LV_ALIGN_TOP_LEFT, 0, 60);

	info_widgets.bat = lv_label_create(lv_scr_act());
	lv_label_set_text(info_widgets.bat, "0000mv");
	lv_obj_align(info_widgets.bat, LV_ALIGN_TOP_LEFT, 32, 16);
	info_widgets.prs = lv_label_create(lv_scr_act());
	lv_label_set_text(info_widgets.prs, "000000Pa");
	lv_obj_align(info_widgets.prs, LV_ALIGN_TOP_LEFT, 32, 26);
	info_widgets.tmp = lv_label_create(lv_scr_act());
	lv_label_set_text(info_widgets.tmp, "00C");
	lv_obj_align(info_widgets.tmp, LV_ALIGN_TOP_LEFT, 40, 36);
	info_widgets.alt = lv_label_create(lv_scr_act());
	lv_label_set_text(info_widgets.alt, "0000m");
	lv_obj_align(info_widgets.alt, LV_ALIGN_TOP_LEFT, 72, 46);
	info_widgets.gnss = lv_label_create(lv_scr_act());
	lv_label_set_long_mode(info_widgets.gnss, LV_LABEL_LONG_WRAP);
	lv_obj_set_width(info_widgets.gnss, 128);
	lv_label_set_text(info_widgets.gnss,
			  "Lat:--\nLon:--\nHDOP:0.0 --\nQ:0\nSat:0\nSpd:0.0 km/h\nCog:0.0 deg");
	lv_obj_align(info_widgets.gnss, LV_ALIGN_TOP_LEFT, 0, 70);

#define INFO_CANVAS_WIDTH  128
#define INFO_CANVAS_HEIGHT  16
	static lv_color_t cbuf[LV_CANVAS_BUF_SIZE_TRUE_COLOR(INFO_CANVAS_WIDTH, INFO_CANVAS_HEIGHT)];
	lv_obj_t *canvas = lv_canvas_create(lv_scr_act());
	lv_canvas_set_buffer(canvas, cbuf, INFO_CANVAS_WIDTH, INFO_CANVAS_HEIGHT,
			     LV_IMG_CF_TRUE_COLOR);
	lv_obj_align(canvas, LV_ALIGN_TOP_MID, 0, 0);
	lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);

	refresh_timer = lv_timer_create(refresh_info, 500, &info_widgets);
	lv_timer_set_repeat_count(refresh_timer, -1);
	lv_timer_ready(refresh_timer);
}

static void loop_info_page(int arg)
{
	ARG_UNUSED(arg);
}

static void teardown_info_page(int arg)
{
	ARG_UNUSED(arg);

	if (refresh_timer != NULL) {
		lv_timer_del(refresh_timer);
		refresh_timer = NULL;
	}

	lv_obj_clean(lv_scr_act());
}

void info_page_register(PageManager *manager, uint8_t page_id)
{
	manager->PageRegister(page_id, setup_info_page, loop_info_page,
			      teardown_info_page, NULL);
}
