#include "gnss.h"

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

static void log_rx_line(void)
{
	if (rx_line_len == 0U) {
		return;
	}

	rx_line[rx_line_len] = '\0';
	LOG_INF("GNSS RX: %s", rx_line);
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
