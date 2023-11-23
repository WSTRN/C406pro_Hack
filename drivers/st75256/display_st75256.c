/*
 * Copyright (c) 2023 Taisheng Wang <wstrn66@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sitronix_st75256

#include "display_st75256.h"

#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/display.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(display_st75256, CONFIG_DISPLAY_LOG_LEVEL);

#define ST75256_RESET_TIME              K_MSEC(1)
#define ST75256_EXIT_SLEEP_TIME K_MSEC(120)


struct st75256_data {
	// uint8_t buf[128*20];
	uint8_t x;
	uint8_t y;
	uint8_t w;
	uint8_t h;
	uint16_t len;
};
struct st75256_config {
	struct spi_dt_spec bus;
	struct gpio_dt_spec cmd_data;
	struct gpio_dt_spec reset;
	uint16_t height;
	uint16_t width;

	uint8_t auto_read[1];
	uint8_t analog_circuit[3];
	uint8_t gray_level[16];
	uint8_t row_range[2];
	uint8_t col_range[2];
	uint8_t data_scan[1];
	uint8_t display_control[3];
	uint8_t color_mode[1];
	uint8_t volume_control[2];
	uint8_t power_control[1];
};


// static void st75256_set_lcd_margins(const struct device *dev,
// 				    uint16_t x_offset, uint16_t y_offset)
// {
// 	struct st75256_data *data = dev->data;

// 	data->x_offset = x_offset;
// 	data->y_offset = y_offset;
// }

static void st75256_set_cmd(const struct device *dev, int is_cmd)
{
	const struct st75256_config *config = dev->config;

	gpio_pin_set_dt(&config->cmd_data, is_cmd);
}

static int st75256_transmit_hold(const struct device *dev, uint8_t cmd,
				 const uint8_t *tx_data, size_t tx_count)
{
	const struct st75256_config *config = dev->config;
	struct spi_buf tx_buf = { .buf = &cmd, .len = 1 };
	struct spi_buf_set tx_bufs = { .buffers = &tx_buf, .count = 1 };
	int ret;

	st75256_set_cmd(dev, 1);
	ret = spi_write_dt(&config->bus, &tx_bufs);
	if (ret < 0) {
		return ret;
	}
	// LOG_INF("send cmd: %x", (uint8_t)(tx_bufs.buffers->buf[0]));
	// LOG_INF("send cmd: %x", (uint8_t)(cmd));

	if (tx_data != NULL) {
		tx_buf.buf = (void *)tx_data;
		tx_buf.len = tx_count;
		st75256_set_cmd(dev, 0);
		ret = spi_write_dt(&config->bus, &tx_bufs);
		if (ret < 0) {
			return ret;
		}
		// for(int i = 0; i < tx_bufs.buffers->len; i++)
		// 	LOG_INF("send data: %x", (uint8_t)(tx_data[i]));
	}
	return 0;
}

static int st75256_transmit(const struct device *dev, uint8_t cmd,
			    const uint8_t *tx_data, size_t tx_count)
{
	const struct st75256_config *config = dev->config;
	int ret;

	ret = st75256_transmit_hold(dev, cmd, tx_data, tx_count);
	spi_release_dt(&config->bus);
	return ret;
}

static int st75256_exit_sleep(const struct device *dev)
{
	int ret;

	ret = st75256_transmit(dev, ST75256_CMD_00_COMMANDS, NULL, 0);
	ret = st75256_transmit(dev, ST75256_CMD_SLEEP_OUT, NULL, 0);
	if (ret < 0) {
		return ret;
	}

	k_sleep(ST75256_EXIT_SLEEP_TIME);

	return 0;
}

static int st75256_sleep(const struct device *dev)
{
	int ret;

	ret = st75256_transmit(dev, ST75256_CMD_00_COMMANDS, NULL, 0);
	ret = st75256_transmit(dev, ST75256_CMD_SLEEP_IN, NULL, 0);
	if (ret < 0) {
		return ret;
	}

	k_sleep(ST75256_EXIT_SLEEP_TIME);

	return 0;
}

static int st75256_reset_display(const struct device *dev)
{
	const struct st75256_config *config = dev->config;

	LOG_DBG("Resetting display");
	if (config->reset.port != NULL) {
		gpio_pin_set_dt(&config->reset, 1);
		k_sleep(ST75256_RESET_TIME);
		gpio_pin_set_dt(&config->reset, 0);
	} else {
		LOG_ERR("No reset GPIO pin available");
		return -ENODEV;
	}

	k_sleep(ST75256_EXIT_SLEEP_TIME);

	return 0;
}

static int st75256_blanking_on(const struct device *dev)
{
	return st75256_transmit(dev, ST75256_CMD_DISPLAY_OFF, NULL, 0);
}

static int st75256_blanking_off(const struct device *dev)
{
	return st75256_transmit(dev, ST75256_CMD_DISPLAY_ON, NULL, 0);
}

static int st75256_read(const struct device *dev,
			const uint16_t x,
			const uint16_t y,
			const struct display_buffer_descriptor *desc,
			void *buf)
{
	return -ENOTSUP;
}

static int st75256_set_mem_area(const struct device *dev,
				const uint16_t x, const uint16_t y,
				const uint16_t w, const uint16_t h)
{
	// const struct st75256_config *config = dev->config;
	// struct st75256_data *data = dev->data;
	uint8_t spi_data[2];

	int ret;
	// LOG_INF("Set area %dx%d (w,h) @ %dx%d (x,y)",
	// 	desc->width, desc->height, x, y);

	st75256_transmit_hold(dev, ST75256_CMD_00_COMMANDS, NULL, 0);
	spi_data[0] = x;
	spi_data[1] = x + w -1;
	ret = st75256_transmit_hold(dev, ST75256_CMD_COL_RANGE, (void *)&spi_data, 2);
	if (ret < 0) {
		return ret;
	}

	spi_data[0] = y/8;
	spi_data[1] = (y + h)/8 - 1;
	ret = st75256_transmit_hold(dev, ST75256_CMD_ROW_RANGE, (void *)&spi_data, 2);
	if (ret < 0) {
		return ret;
	}
	
	// /* ST7735S requires repeating COLMOD for each transfer */
	// ret = st75256_transmit_hold(dev, ST75256_CMD_COLMOD, &config->colmod, 1);
	// if (ret < 0) {
	// 	return ret;
	// }

	// uint16_t ram_x = x + data->x_offset;
	// uint16_t ram_y = y + data->y_offset;

	// spi_data[0] = sys_cpu_to_be16(ram_x);
	// spi_data[1] = sys_cpu_to_be16(ram_x + w - 1);
	// ret = st75256_transmit_hold(dev, ST75256_CMD_CASET, (uint8_t *)&spi_data[0], 4);
	// if (ret < 0) {
	// 	return ret;
	// }

	// spi_data[0] = sys_cpu_to_be16(ram_y);
	// spi_data[1] = sys_cpu_to_be16(ram_y + h - 1);
	// ret = st75256_transmit_hold(dev, ST75256_CMD_RASET, (uint8_t *)&spi_data[0], 4);
	// if (ret < 0) {
	// 	return ret;
	// }

	/* NB: CS still held - data transfer coming next */
	return 0;
}

static int st75256_write(const struct device *dev,
			 const uint16_t x,
			 const uint16_t y,
			 const struct display_buffer_descriptor *desc,
			 const void *buf)
{
	const struct st75256_config *config = dev->config;
	const uint8_t *write_data_start = (uint8_t *) buf;
	// struct spi_buf tx_buf;
	// struct spi_buf_set tx_bufs;
	// uint16_t write_cnt;
	// uint16_t nbr_of_writes;
	// uint16_t write_h;
	int ret;

	__ASSERT(desc->width <= desc->pitch, "Pitch is smaller than width");
	__ASSERT((desc->width * desc->height)
		 <= desc->buf_size * 8, "Input buffer too small");

	// LOG_INF("Writing %dx%d (w,h) @ %dx%d (x,y)",esc->width, desc->height, x, y);
	ret = st75256_set_mem_area(dev, x, y, desc->width, desc->height);
	if (ret < 0) {
		goto out;
	}

	// if (desc->pitch > desc->width) {
	// 	write_h = 1U;
	// 	nbr_of_writes = desc->height;
	// } else {
	// 	write_h = desc->height;
	// 	nbr_of_writes = 1U;
	// }
	
	ret = st75256_transmit_hold(dev, ST75256_CMD_WRITE_DATA,
				    (void *) write_data_start,
				    (desc->width) * (desc->height)/8);
	if (ret < 0) {
		goto out;
	}

	// tx_bufs.buffers = &tx_buf;
	// tx_bufs.count = 1;

	// write_data_start += (desc->pitch * ST75256_PIXEL_SIZE);
	// for (write_cnt = 1U; write_cnt < nbr_of_writes; ++write_cnt) {
	// 	tx_buf.buf = (void *)write_data_start;
	// 	tx_buf.len = desc->width * ST75256_PIXEL_SIZE * write_h;
	// 	ret = spi_write_dt(&config->bus, &tx_bufs);
	// 	if (ret < 0) {
	// 		goto out;
	// 	}

	// 	write_data_start += (desc->pitch * ST75256_PIXEL_SIZE);
	// }

	ret = 0;
out:
	spi_release_dt(&config->bus);
	return ret;



// 	__ASSERT(desc->width <= desc->pitch, "Pitch is smaller than width");
// 	__ASSERT((desc->pitch * ST75256_PIXEL_SIZE * desc->height)
// 		 <= desc->buf_size, "Input buffer too small");

// 	LOG_DBG("Writing %dx%d (w,h) @ %dx%d (x,y)",
// 		desc->width, desc->height, x, y);
// 	ret = st75256_set_mem_area(dev, x, y, desc->width, desc->height);
// 	if (ret < 0) {
// 		goto out;
// 	}

// 	if (desc->pitch > desc->width) {
// 		write_h = 1U;
// 		nbr_of_writes = desc->height;
// 	} else {
// 		write_h = desc->height;
// 		nbr_of_writes = 1U;
// 	}

// 	ret = st75256_transmit_hold(dev, ST75256_CMD_RAMWR,
// 				    (void *) write_data_start,
// 				    desc->width * ST75256_PIXEL_SIZE * write_h);
// 	if (ret < 0) {
// 		goto out;
// 	}

// 	tx_bufs.buffers = &tx_buf;
// 	tx_bufs.count = 1;

// 	write_data_start += (desc->pitch * ST75256_PIXEL_SIZE);
// 	for (write_cnt = 1U; write_cnt < nbr_of_writes; ++write_cnt) {
// 		tx_buf.buf = (void *)write_data_start;
// 		tx_buf.len = desc->width * ST75256_PIXEL_SIZE * write_h;
// 		ret = spi_write_dt(&config->bus, &tx_bufs);
// 		if (ret < 0) {
// 			goto out;
// 		}

// 		write_data_start += (desc->pitch * ST75256_PIXEL_SIZE);
// 	}

// 	ret = 0;
// out:
// 	spi_release_dt(&config->bus);
// 	return ret;
}

static void *st75256_get_framebuffer(const struct device *dev)
{
	return NULL;
}

static int st75256_set_brightness(const struct device *dev,
				  const uint8_t brightness)
{
	return -ENOTSUP;
}

static int st75256_set_contrast(const struct device *dev,
				const uint8_t contrast)
{
	return -ENOTSUP;
}

static void st75256_get_capabilities(const struct device *dev,
				     struct display_capabilities *capabilities)
{
	const struct st75256_config *config = dev->config;

	memset(capabilities, 0, sizeof(struct display_capabilities));
	capabilities->x_resolution = config->width;
	capabilities->y_resolution = config->height;

	/*
	 * Invert the pixel format if rgb_is_inverted is enabled.
	 * Report pixel format as the same format set in the MADCTL
	 * if disabling the rgb_is_inverted option.
	 * Or not so, reporting pixel format as RGB if MADCTL setting
	 * is BGR. And also vice versa.
	 * It is a workaround for supporting buggy modules that display RGB as BGR.
	 */
	// if (!(config->madctl & ST75256_MADCTL_BGR) != !config->rgb_is_inverted) {
	// 	capabilities->supported_pixel_formats = PIXEL_FORMAT_BGR_565;
	// 	capabilities->current_pixel_format = PIXEL_FORMAT_BGR_565;
	// } else {
	// 	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
	// 	capabilities->current_pixel_format = PIXEL_FORMAT_RGB_565;
	// }
	capabilities->supported_pixel_formats = PIXEL_FORMAT_MONO01;
	capabilities->current_pixel_format = PIXEL_FORMAT_MONO01;
	capabilities->screen_info = SCREEN_INFO_MONO_VTILED;
	capabilities->current_orientation = DISPLAY_ORIENTATION_NORMAL;
}

static int st75256_set_pixel_format(const struct device *dev,
				    const enum display_pixel_format pixel_format)
{
	// const struct st75256_config *config = dev->config;

	// if ((pixel_format == PIXEL_FORMAT_RGB_565) &&
	//     (~config->madctl & ST75256_MADCTL_BGR)) {
	// 	return 0;
	// }

	// if ((pixel_format == PIXEL_FORMAT_BGR_565) &&
	//     (config->madctl & ST75256_MADCTL_BGR)) {
	// 	return 0;
	// }
	
	LOG_ERR("Pixel format change not implemented");

	return -ENOTSUP;
}

static int st75256_set_orientation(const struct device *dev,
				   const enum display_orientation orientation)
{
	if (orientation == DISPLAY_ORIENTATION_NORMAL) {
		return 0;
	}

	LOG_ERR("Changing display orientation not implemented");

	return -ENOTSUP;
}
// uint8_t tbuf[20*128];
static int st75256_lcd_init(const struct device *dev)
{
	const struct st75256_config *config = dev->config;
	// struct st75256_data *data = dev->data;
	int ret;

	// st75256_set_lcd_margins(dev, data->x_offset, data->y_offset);

	ret = st75256_transmit(dev, ST75256_CMD_00_COMMANDS, NULL, 0);
	if (ret < 0) {
		return ret;
	}
	ret = st75256_transmit(dev, ST75256_CMD_DISPLAY_OFF, NULL, 0);
	if (ret < 0) {
		return ret;
	}

	ret = st75256_transmit(dev, ST75256_CMD_01_COMMANDS, NULL, 0);
	if (ret < 0) {
		return ret;
	}
	ret = st75256_transmit(dev, ST75256_CMD_AUTO_READ, config->auto_read, sizeof(config->auto_read));
	if (ret < 0) {
		return ret;
	}
	ret = st75256_transmit(dev, ST75256_CMD_ANALOG_CIRCUIT, config->analog_circuit, sizeof(config->analog_circuit));
	if (ret < 0) {
		return ret;
	}
	ret = st75256_transmit(dev, ST75256_CMD_GRAY_LEVEL, config->gray_level, sizeof(config->gray_level));
	if (ret < 0) {
		return ret;
	}

	ret = st75256_transmit(dev, ST75256_CMD_00_COMMANDS, NULL, 0);
	if (ret < 0) {
		return ret;
	}

	ret = st75256_transmit(dev, ST75256_CMD_DATA_SCAN, config->data_scan, sizeof(config->data_scan));
	if (ret < 0) {
		return ret;
	}
	ret = st75256_transmit(dev, ST75256_CMD_DATA_FORMAT_LSB, NULL, 0);
	if (ret < 0) {
		return ret;
	}
	ret = st75256_transmit(dev, ST75256_CMD_DISPLAY_CONTROL, config->display_control, sizeof(config->display_control));
	if (ret < 0) {
		return ret;
	}
	ret = st75256_transmit(dev, ST75256_CMD_COLOR_MODE, config->color_mode, sizeof(config->color_mode));
	if (ret < 0) {
		return ret;
	}
	ret = st75256_transmit(dev, ST75256_CMD_VOLUME_CONTROL, config->volume_control, sizeof(config->volume_control));
	if (ret < 0) {
		return ret;
	}
	ret = st75256_transmit(dev, ST75256_CMD_POWER_CONTROL, config->power_control, sizeof(config->power_control));
	if (ret < 0) {
		return ret;
	}
	LOG_INF("st75256 initalized");
	
	ret = st75256_transmit(dev, ST75256_CMD_COL_RANGE, config->row_range, sizeof(config->row_range));
	if (ret < 0) {
		return ret;
	}
	ret = st75256_transmit(dev, ST75256_CMD_ROW_RANGE, config->col_range, sizeof(config->col_range));
	if (ret < 0) {
		return ret;
	}
	// st75256_transmit_hold(dev, ST75256_CMD_00_COMMANDS, NULL, 0);
	// ret = st75256_transmit_hold(dev, ST75256_CMD_WRITE_DATA, data->buf,128*20);
	// spi_release_dt(&config->bus);

	return 0;
}

static int st75256_init(const struct device *dev)
{
	const struct st75256_config *config = dev->config;
	int ret;

	// static const struct device *gpio_0;
	// gpio_0 = device_get_binding("gpio@50000000");
	// gpio_pin_configure(gpio_0, 8, GPIO_OUTPUT_ACTIVE);
	// ret = gpio_pin_set(gpio_0, 8, 1);//power on
	// if (ret < 0) {
	// 	return ret;
	// }
	// LOG_INF("Power on");

	if (!spi_is_ready_dt(&config->bus)) {
		LOG_ERR("SPI bus %s not ready", config->bus.bus->name);
		return -ENODEV;
	}

	if (config->reset.port != NULL) {
		if (!gpio_is_ready_dt(&config->reset)) {
			LOG_ERR("Reset GPIO port for display not ready");
			return -ENODEV;
		}
		ret = gpio_pin_configure_dt(&config->reset,
					    GPIO_OUTPUT_INACTIVE);
		if (ret) {
			LOG_ERR("Couldn't configure reset pin");
			return ret;
		}
	}
	if (!gpio_is_ready_dt(&config->cmd_data)) {
		LOG_ERR("cmd/DATA GPIO port not ready");
		return -ENODEV;
	}
	ret = gpio_pin_configure_dt(&config->cmd_data, GPIO_OUTPUT);
	if (ret) {
		LOG_ERR("Couldn't configure cmd/DATA pin");
		return ret;
	}

	ret = st75256_reset_display(dev);
	if (ret < 0) {
		LOG_ERR("Couldn't reset display");
		return ret;
	}

	ret = st75256_exit_sleep(dev);
	if (ret < 0) {
		LOG_ERR("Couldn't exit sleep");
		return ret;
	}

	ret = st75256_lcd_init(dev);
	if (ret < 0) {
		LOG_ERR("Couldn't init LCD");
		return ret;
	}
	return 0;
}

#ifdef CONFIG_PM_DEVICE
static int st75256_pm_action(const struct device *dev,
			     enum pm_device_action action)
{
	int ret = 0;

	switch (action) {
	case PM_DEVICE_ACTION_RESUME:
		LOG_INF("resume");
		st75256_exit_sleep(dev);
		break;
	case PM_DEVICE_ACTION_SUSPEND:
		LOG_INF("suspend");
		st75256_sleep(dev);
		break;
	case PM_DEVICE_ACTION_TURN_OFF:
		LOG_INF("turn off");
		break;
	case PM_DEVICE_ACTION_TURN_ON:
		st75256_init(dev);
		LOG_INF("turn on");
		break;
	default:
		ret = -ENOTSUP;
		break;
	}

	return ret;
}
#endif /* CONFIG_PM_DEVICE */


static const struct display_driver_api st75256_api = {
	.blanking_on = st75256_blanking_on,
	.blanking_off = st75256_blanking_off,
	.write = st75256_write,
	.read = st75256_read,
	.get_framebuffer = st75256_get_framebuffer,
	.set_brightness = st75256_set_brightness,
	.set_contrast = st75256_set_contrast,
	.get_capabilities = st75256_get_capabilities,
	.set_pixel_format = st75256_set_pixel_format,
	.set_orientation = st75256_set_orientation,
};


#define ST75256_INIT(inst) \
	\
	const static struct st75256_config st75256_config_##inst = {		\
		.bus = SPI_DT_SPEC_INST_GET(					\
			inst, SPI_OP_MODE_MASTER | SPI_WORD_SET(8) |		\
			SPI_HOLD_ON_CS | SPI_LOCK_ON, 0),			\
		.cmd_data = GPIO_DT_SPEC_INST_GET(inst, cmd_data_gpios),	\
		.reset = GPIO_DT_SPEC_INST_GET_OR(inst, reset_gpios, {}),	\
		.width = DT_INST_PROP(inst, width),				\
		.height = DT_INST_PROP(inst, height),				\
		.row_range = DT_INST_PROP(inst, row_range),			\
		.col_range = DT_INST_PROP(inst, col_range),			\
		.color_mode = DT_INST_PROP(inst, color_mode),			\
		.auto_read = DT_INST_PROP(inst, auto_read),			\
		.analog_circuit = DT_INST_PROP(inst, analog_circuit),		\
		.gray_level = DT_INST_PROP(inst, gray_level),			\
		.data_scan = DT_INST_PROP(inst, data_scan),			\
		.display_control = DT_INST_PROP(inst, display_control),		\
		.volume_control = DT_INST_PROP(inst, volume_control),		\
		.power_control = DT_INST_PROP(inst, power_control),		\
	}; \
	static struct st75256_data st75256_data_##inst = {0}; \
	\
	PM_DEVICE_DT_INST_DEFINE(inst, st75256_pm_action);	\
	\
	DEVICE_DT_INST_DEFINE(inst,\
				st75256_init,\
				PM_DEVICE_DT_INST_GET(inst),\
			    &st75256_data_##inst,\
				&st75256_config_##inst,\
			    POST_KERNEL,\
				CONFIG_DISPLAY_INIT_PRIORITY,\
			    &st75256_api\
	);

DT_INST_FOREACH_STATUS_OKAY(ST75256_INIT)
