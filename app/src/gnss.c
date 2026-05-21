#include "gnss.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(gnss, LOG_LEVEL_INF);

#define GNSS_UART_NODE DT_ALIAS(gnss_uart)
#define GNSS_RESET_PIN 6
#define GNSS_BAUD_RATE 115200
#define RX_LINE_LEN    128

BUILD_ASSERT(DT_NODE_HAS_STATUS(GNSS_UART_NODE, okay),
	     "gnss-uart alias must point to an enabled UART");

static const struct device *const gnss_uart = DEVICE_DT_GET(GNSS_UART_NODE);
static const struct device *const gpio0_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

static char rx_line[RX_LINE_LEN];
static size_t rx_line_len;
static struct gnss_info gnss_info_cache;

static bool nmea_match(const char *sentence, const char *type)
{
	size_t type_len = strlen(type);

	return sentence[0] == '$' && strncmp(&sentence[3], type, type_len) == 0;
}

static bool nmea_get_field(const char *sentence, uint8_t field_index, char *out, size_t out_size)
{
	uint8_t current = 0U;
	const char *field_start = sentence + 1;
	const char *p = sentence;

	if (out_size == 0U) {
		return false;
	}

	while (*p != '\0') {
		if (*p == ',' || *p == '*') {
			if (current == field_index) {
				size_t len = (size_t)(p - field_start);

				if (len >= out_size) {
					len = out_size - 1U;
				}

				memcpy(out, field_start, len);
				out[len] = '\0';
				return true;
			}

			if (*p == '*') {
				break;
			}

			current++;
			field_start = p + 1;
		}

		p++;
	}

	if (current == field_index) {
		size_t len = strlen(field_start);

		if (len >= out_size) {
			len = out_size - 1U;
		}

		memcpy(out, field_start, len);
		out[len] = '\0';
		return true;
	}

	return false;
}

static bool nmea_to_e6(const char *value, char hemi, bool is_latitude, int32_t *coord_e6)
{
	char *dot;
	long degrees;
	long minutes_whole;
	long minutes_frac = 0;
	long frac_scale = 1;
	long total_micro;
	char buf[24];

	if (value[0] == '\0') {
		return false;
	}

	strncpy(buf, value, sizeof(buf) - 1U);
	buf[sizeof(buf) - 1U] = '\0';

	dot = strchr(buf, '.');
	if (dot != NULL) {
		char *frac = dot + 1;

		*dot = '\0';
		while (*frac != '\0' && frac_scale < 10000L) {
			minutes_frac = (minutes_frac * 10L) + (*frac - '0');
			frac_scale *= 10L;
			frac++;
		}
	}

	minutes_whole = strtol(buf + (is_latitude ? 2 : 3), NULL, 10);
	buf[is_latitude ? 2 : 3] = '\0';
	degrees = strtol(buf, NULL, 10);

	total_micro = degrees * 1000000L;
	total_micro += (minutes_whole * 1000000L) / 60L;
	total_micro += (minutes_frac * 1000000L) / (60L * frac_scale);

	if (hemi == 'S' || hemi == 'W') {
		total_micro = -total_micro;
	}

	*coord_e6 = (int32_t)total_micro;
	return true;
}

static bool nmea_decimal_to_x10(const char *value, uint16_t *out_x10)
{
	char buf[16];
	char *dot;
	unsigned long whole;
	unsigned long frac = 0;

	if (value[0] == '\0') {
		return false;
	}

	strncpy(buf, value, sizeof(buf) - 1U);
	buf[sizeof(buf) - 1U] = '\0';
	dot = strchr(buf, '.');
	if (dot != NULL) {
		*dot = '\0';
		if (*(dot + 1) != '\0') {
			frac = (unsigned long)(*(dot + 1) - '0');
		}
	}

	whole = strtoul(buf, NULL, 10);
	*out_x10 = (uint16_t)(whole * 10UL + frac);
	return true;
}

static void gnss_parse_gga(const char *sentence)
{
	char lat[24];
	char lat_hemi[4];
	char lon[24];
	char lon_hemi[4];
	char quality[8];
	char sats[8];
	char hdop[16];
	struct gnss_info parsed;
	unsigned int key;

	key = irq_lock();
	parsed = gnss_info_cache;
	irq_unlock(key);

	parsed.has_fix = false;
	parsed.lat_e6 = 0;
	parsed.lon_e6 = 0;
	parsed.quality = 0U;
	parsed.satellites = 0U;
	parsed.hdop_x10 = 0U;

	if (!nmea_get_field(sentence, 2, lat, sizeof(lat)) ||
	    !nmea_get_field(sentence, 3, lat_hemi, sizeof(lat_hemi)) ||
	    !nmea_get_field(sentence, 4, lon, sizeof(lon)) ||
	    !nmea_get_field(sentence, 5, lon_hemi, sizeof(lon_hemi)) ||
	    !nmea_get_field(sentence, 6, quality, sizeof(quality)) ||
	    !nmea_get_field(sentence, 7, sats, sizeof(sats)) ||
	    !nmea_get_field(sentence, 8, hdop, sizeof(hdop))) {
		return;
	}

	parsed.quality = (uint8_t)strtoul(quality, NULL, 10);
	parsed.satellites = (uint8_t)strtoul(sats, NULL, 10);
	(void)nmea_decimal_to_x10(hdop, &parsed.hdop_x10);
	parsed.has_fix = parsed.quality > 0U;

	if (parsed.has_fix) {
		if (!nmea_to_e6(lat, lat_hemi[0], true, &parsed.lat_e6) ||
		    !nmea_to_e6(lon, lon_hemi[0], false, &parsed.lon_e6)) {
			parsed.has_fix = false;
		}
	}

	key = irq_lock();
	gnss_info_cache = parsed;
	irq_unlock(key);
}

static void gnss_parse_rmc(const char *sentence)
{
	char speed_knots[16];
	char course_deg[16];
	uint16_t speed_knots_x10;
	uint16_t course_deg_x10;
	unsigned int key;

	if (!nmea_get_field(sentence, 7, speed_knots, sizeof(speed_knots)) ||
	    !nmea_get_field(sentence, 8, course_deg, sizeof(course_deg))) {
		return;
	}

	if (!nmea_decimal_to_x10(speed_knots, &speed_knots_x10) ||
	    !nmea_decimal_to_x10(course_deg, &course_deg_x10)) {
		return;
	}

	key = irq_lock();
	gnss_info_cache.speed_kmh_x10 = (uint16_t)((speed_knots_x10 * 1852UL) / 1000UL);
	gnss_info_cache.course_deg_x10 = course_deg_x10;
	irq_unlock(key);
}

static void log_rx_line(void)
{
	if (rx_line_len == 0U) {
		return;
	}

	rx_line[rx_line_len] = '\0';
	LOG_INF("GNSS RX: %s", rx_line);

	if (nmea_match(rx_line, "GGA")) {
		gnss_parse_gga(rx_line);
	} else if (nmea_match(rx_line, "RMC")) {
		gnss_parse_rmc(rx_line);
	}

	rx_line_len = 0U;
}

static void gnss_uart_isr(const struct device *dev, void *user_data)
{
	uint8_t buf[16];

	ARG_UNUSED(user_data);

	if (!uart_irq_update(dev)) {
		return;
	}

	while (uart_irq_rx_ready(dev)) {
		int read = uart_fifo_read(dev, buf, sizeof(buf));

		if (read <= 0) {
			break;
		}

		for (int i = 0; i < read; i++) {
			uint8_t ch = buf[i];

			if (ch == '\r' || ch == '\n') {
				log_rx_line();
				continue;
			}

			if (rx_line_len >= (RX_LINE_LEN - 1U)) {
				LOG_WRN("GNSS line too long, truncating");
				log_rx_line();
			}

			rx_line[rx_line_len++] = (char)ch;
		}
	}
}

static int gnss_uart_setup(void)
{
	if (!device_is_ready(gnss_uart)) {
		LOG_ERR("GNSS UART device not ready");
		return -1;
	}

	int ret = uart_irq_callback_user_data_set(gnss_uart, gnss_uart_isr, NULL);
	if (ret < 0) {
		LOG_ERR("uart_irq_callback_user_data_set failed: %d", ret);
		return ret;
	}

	uart_irq_rx_enable(gnss_uart);
	LOG_INF("GNSS UART ready on uart1 (board default: %d 8N1)", GNSS_BAUD_RATE);

	return 0;
}

static void gnss_reset_release(void)
{
	int ret;

	if (!device_is_ready(gpio0_dev)) {
		LOG_WRN("GPIO0 not ready, skip UC6226 reset");
		return;
	}

	ret = gpio_pin_configure(gpio0_dev, GNSS_RESET_PIN, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_WRN("Failed to drive UC6226 reset high: %d", ret);
		return;
	}

	LOG_INF("UC6226 reset released on P0.06");
}

void gnss_init(void)
{
	LOG_INF("GNSS UART logger start");

	gnss_reset_release();

	if (gnss_uart_setup() < 0) {
		LOG_ERR("GNSS UART init failed");
		return;
	}

	LOG_INF("Waiting for GNSS NMEA data...");
}

void gnss_get_info(struct gnss_info *info)
{
	unsigned int key;

	if (info == NULL) {
		return;
	}

	key = irq_lock();
	*info = gnss_info_cache;
	irq_unlock(key);
}
