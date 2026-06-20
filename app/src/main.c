#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/display.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/poweroff.h>
#include <lvgl.h>
#include <zephyr/drivers/sensor.h>

#include "main_page.h"
#include "battery.h"
#include "button.h"
#include "ble/ble.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

const struct device *gpio_0;
const struct device *gpio_1;
const struct device *display_dev;
const struct device *ext_power;
const struct device *pressure_dev;


int main()
{
	LOG_INF("GPIO TEST!!!!!!!");

    gpio_0 = device_get_binding("gpio@50000000");
	gpio_1 = device_get_binding("gpio@50000300");
	ext_power = DEVICE_DT_GET(DT_NODELABEL(powerdomain0));
	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	pressure_dev = DEVICE_DT_GET(DT_NODELABEL(sensor0));
	
	gpio_pin_configure(gpio_0, 29, GPIO_INPUT);
	gpio_pin_configure(gpio_0, 31, GPIO_INPUT);
	gpio_pin_configure(gpio_1, 10, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure(gpio_0, 9, GPIO_OUTPUT_INACTIVE);

	battery_sensor_init();
	ButtonEvent_Init();
	main_page();
	ble_start();
	

	while (1) {
		k_msleep(100);
	}

	return 0;
}
