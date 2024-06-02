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
#include "gnss.h"

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

	int err;

    gpio_0 = device_get_binding("gpio@50000000");
	gpio_1 = device_get_binding("gpio@50000300");
	ext_power = DEVICE_DT_GET(DT_NODELABEL(powerdomain0));
	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	pressure_dev = DEVICE_DT_GET(DT_NODELABEL(sensor0));
	
	gpio_pin_configure(gpio_0, 29, GPIO_INPUT);
	gpio_pin_configure(gpio_0, 31, GPIO_INPUT);
	gpio_pin_configure(gpio_1, 10, GPIO_OUTPUT_INACTIVE);

	battery_sensor_init();
	ButtonEvent_Init();
	main_page();
	k_msleep(200);
	gnss_init();
	
	// display_blanking_off(display_dev);
	// k_msleep(4000);
	// err = pm_device_action_run(display_dev, PM_DEVICE_ACTION_SUSPEND);
	// LOG_INF("%d",err);
	// err = pm_device_action_run(ext_power, PM_DEVICE_ACTION_SUSPEND);
	// LOG_INF("%d",err);
	// k_msleep(10000);
	// err = pm_device_action_run(ext_power, PM_DEVICE_ACTION_RESUME);
	// LOG_INF("%d",err);
	// err = pm_device_action_run(display_dev, PM_DEVICE_ACTION_RESUME);
	// LOG_INF("%d",err);
	// display_blanking_off(display_dev);
	// lv_label_set_text(obj, "1234567 Test");
	// lv_task_handler();

	// gpio_pin_configure_dt(&btn0, GPIO_INPUT);
	// gpio_pin_interrupt_configure_dt(&btn0,  GPIO_INT_LEVEL_ACTIVE);
	// gpio_init_callback(&btn0_cb_data, btn0_pressed, BIT(btn0.pin));
	// gpio_add_callback(btn0.port, &btn0_cb_data);
	
	// gpio_pin_configure_dt(&btn1, GPIO_INPUT);
	// gpio_pin_interrupt_configure_dt(&btn1, GPIO_INT_EDGE_BOTH);
	// gpio_init_callback(&btn1_cb_data, btn1_pressed, BIT(btn1.pin));
	// gpio_add_callback(btn1.port, &btn1_cb_data);

	// gpio_pin_configure_dt(&btn2, GPIO_INPUT);
	// gpio_pin_interrupt_configure_dt(&btn2, GPIO_INT_EDGE_BOTH);
	// gpio_init_callback(&btn2_cb_data, btn2_pressed, BIT(btn2.pin));
	// gpio_add_callback(btn2.port, &btn2_cb_data);


	// k_msleep(20000);
	// err = pm_device_action_run(display_dev, PM_DEVICE_ACTION_SUSPEND);
	// LOG_INF("%d",err);
	// err = pm_device_action_run(ext_power, PM_DEVICE_ACTION_SUSPEND);
	// LOG_INF("%d",err);
	// sys_poweroff();

	while (1) {
		k_msleep(100);
		// LOG_INF("Entering system off; press sw0 to restart");
		// sys_poweroff();
	}

	return 0;



	// k_msleep(4000);

	// LOG_INF("Display sample for %s", display_dev->name);
	// size_t buf_size = 128*10;
	// struct display_buffer_descriptor buf_desc;
	// (void)memset(dbuf, 0xff, buf_size);
	// buf_desc.buf_size = buf_size;
	// buf_desc.pitch = 128;
	// buf_desc.width = 64;
	// buf_desc.height = 80;
	// display_write(display_dev, 0, 0, &buf_desc, dbuf);
	// display_blanking_off(display_dev);
	// display_write(display_dev, 32, 40, &buf_desc, dbuf);

	// for(int i = 0; i < 32; i++)
	// {
	// 	if(i==29 || i ==31)
	// 	{
	// 		gpio_pin_configure(gpio_0, i, GPIO_INPUT);
	// 		continue;
	// 	}		
	// 	gpio_pin_configure(gpio_0, i, GPIO_OUTPUT_ACTIVE);
	// 	gpio_pin_set(gpio_0, i, 0);
	// }
	// for(int i = 0; i < 16; i++)
	// {
	// 	gpio_pin_configure(gpio_1, i, GPIO_OUTPUT_ACTIVE);
	// 	gpio_pin_set(gpio_1, i, 0);
	// }

	//scan pins
	// for(int i = 0; i < 32; i++)
	// {
	// 	if(i==4 || i==6 || i==8 || i==9 ||
	// 		i==11 || i==12 || i==13 || i==14 ||
	// 		i==15 || i==20 || i==22 || 
	// 		i==24 || i==26 || i==29)
	// 		continue;
	// 	gpio_pin_set(gpio_0, i, 1);
	// 	printk("TEST GPIO %d\n\r", i);
	// 	k_msleep(2000);
	// 	gpio_pin_set(gpio_0, i, 0);
	// }
	// for(int i = 0; i < 16; i++)
	// {
	// 	if(i==0 || i==4 || i==6 || i==7 ||
	// 		i==9 || i==10 || i==11 || i==13 || i==15)
	// 		continue;
	// 	gpio_pin_set(gpio_1, i, 1);
	// 	printk("TEST GPIO %d\n\r", i);
	// 	k_msleep(2000);
	// 	gpio_pin_set(gpio_1, i, 0);
	// }
	
	// for(;;)
	// {
	// 	gpio_pin_toggle(gpio_0, 26);//lcd_sda miso p0.27
	// 	gpio_pin_toggle(gpio_0, 4);//lcd_scl
	// 	gpio_pin_toggle(gpio_1, 7);//lcd_rst
	// 	gpio_pin_toggle(gpio_1, 4);//lcd_dc
	// 	gpio_pin_toggle(gpio_1, 6);//lcd_cs
	// 	gpio_pin_toggle(gpio_1, 10);//lcd_bl
	// 	gpio_pin_toggle(gpio_0, 8);//power
	// 	gpio_pin_toggle(gpio_0, 9);//beep
	// 	gpio_pin_toggle(gpio_0, 20);//flash_hold
	// 	gpio_pin_toggle(gpio_0, 22);//flash_clk
	// 	gpio_pin_toggle(gpio_0, 24);//flash_mosi
	// 	gpio_pin_toggle(gpio_0, 15);//flash_wp
	// 	gpio_pin_toggle(gpio_0, 14);//flash_miso
	// 	gpio_pin_toggle(gpio_0, 13);//flash_cs
	// 	gpio_pin_toggle(gpio_0, 6);//uc6226_rst
	// 	gpio_pin_toggle(gpio_1, 11);//uc6226_rxd
	// 	gpio_pin_toggle(gpio_1, 13);//uc6226_txd
	// 	gpio_pin_toggle(gpio_0, 11);//sw1
	// 	gpio_pin_toggle(gpio_0, 12);//sw2
	// 	gpio_pin_toggle(gpio_1, 15);//sw3
	// 	gpio_pin_toggle(gpio_0, 25);//spl06_sck
	// 	gpio_pin_toggle(gpio_1, 0);//spl06_sda
	// 	gpio_pin_toggle(gpio_0, 29);//bat_adc X4
	// 	gpio_pin_toggle(gpio_0, 31);//charger
	// 	printk("GPIO TEST!!!!!!!\n\r");
	// 	display_blanking_on(display_dev);
	// 	k_msleep(1000);
	// 	display_blanking_off(display_dev);
	// 	k_msleep(1000);
	// }
	// return 0;
}

// void btn0_pressed(const struct device *dev, struct gpio_callback *cb,
// 		    uint32_t pins)
// {
// 	if (gpio_pin_get(btn0.port, btn0.pin)) {
//         printk("Button0 pressed...\r\n");
//     } else {
//         printk("Button0 released...\r\n");
//         // prepare_wake_up();
//         // power_down();
//     }
// }
// void btn1_pressed(const struct device *dev, struct gpio_callback *cb,
// 		    uint32_t pins)
// {
// 	if (gpio_pin_get(btn0.port, btn0.pin)) {
//         printk("Button1 pressed...\r\n");
//     } else {
//         printk("Button1 released...\r\n");
//         // prepare_wake_up();
//         // power_down();
//     }
// 	printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
// }
// void btn2_pressed(const struct device *dev, struct gpio_callback *cb,
// 		    uint32_t pins)
// {
// 	if (gpio_pin_get(btn0.port, btn0.pin)) {
//         printk("Button2 pressed...\r\n");
//     } else {
//         printk("Button2 released...\r\n");
//         // prepare_wake_up();
//         // power_down();
//     }
// 	printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
// }
