/*
 * Copyright (c) 2023 Taisheng WANG <wstrn66@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT goertek_spl06

#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/init.h>
#include <string.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <stdlib.h>
#include "spl06.h"

LOG_MODULE_REGISTER(spl06, CONFIG_SENSOR_LOG_LEVEL);

struct spl06_data {
	const struct device *i2c_dev;
	int prs_cefs[7];
	int tmp_cefs[3];
	float k_prs;
	float k_tmp;
	float prs_data;
	float tmp_data;
	uint8_t prs_raw[3];
	uint8_t tmp_raw[3];
};

struct spl06_config {
	struct i2c_dt_spec i2c;
    uint8_t prs_cfg;
    uint8_t tmp_cfg;
    uint8_t cfg_reg;
    uint8_t meas_cfg;
};

/**
 * @brief sensor value get
 *
 * @return -ENOTSUP for unsupported channels
 */
static int spl06_channel_get(const struct device *dev,
			      enum sensor_channel chan,
			      struct sensor_value *val)
{
	struct spl06_data *data = dev->data;

	float Pressure;
	int Press;
	int Pressure_Data = (data->prs_raw[2]<<16)+(data->prs_raw[1]<<8)+data->prs_raw[0];
	// Pressure_Data = Pressure_Data << 1;
	if(Pressure_Data&0x800000)
	{
		Press = Pressure_Data-Total_Number_24;
	}
	else
	{
		Press = Pressure_Data;
	}
	Pressure = Press/data->k_prs;

	float Temperature;
	int Temp;
	int Temp_Data = (data->tmp_raw[2]<<16)+(data->tmp_raw[1]<<8)+data->tmp_raw[0];
	if(Temp_Data&0x800000)
	{
		Temp = Temp_Data-Total_Number_24;
	}
	else
	{
		Temp = Temp_Data;
	}
	Temperature = Temp/data->k_tmp;

	data->prs_data = data->prs_cefs[0]
					+Pressure*(data->prs_cefs[1]
						+Pressure*(data->prs_cefs[4]
							+Pressure*data->prs_cefs[6]))
					+Temperature*data->prs_cefs[2]
					+Temperature*Pressure*(data->prs_cefs[3]+Pressure*data->prs_cefs[5]);

	data->tmp_data = data->tmp_cefs[0]*0.5+data->tmp_cefs[1]*Temperature;
	
	// LOG_INF("prs = %d, tmp = %d", (int)data->prs_data, (int)data->tmp_data);

	switch (chan) {
	case SENSOR_CHAN_AMBIENT_TEMP:
		val->val1 = data->tmp_data;
		val->val2 = (data->tmp_data - val->val1) * 1000000;
		break;
	case SENSOR_CHAN_PRESS:
		val->val1 = data->prs_data;
		val->val2 = (data->prs_data - val->val1) * 1000000;
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

/**
 * @brief fetch a sample from the sensor
 *
 * @return 0
 */
static int spl06_sample_fetch(const struct device *dev,
			       enum sensor_channel chan)
{
	const struct spl06_config *config = dev->config;
	struct spl06_data *data = dev->data;
	int ret;

	// ret = i2c_reg_write_byte_dt(&config->i2c, SPL06_REG_MEAS_CFG, 0x01);
    // if (ret < 0) {
	// 	return ret;
	// }
	// k_msleep(250);
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_PRS_B0, data->prs_raw);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_PRS_B1, data->prs_raw+1);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_PRS_B2, data->prs_raw+2);
    if (ret < 0) {
		return ret;
	}

	// ret = i2c_reg_write_byte_dt(&config->i2c, SPL06_REG_MEAS_CFG, 0x02);
    // if (ret < 0) {
	// 	return ret;
	// }
	// k_msleep(20);
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_TMP_B0, data->tmp_raw);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_TMP_B1, data->tmp_raw+1);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_TMP_B2, data->tmp_raw+2);
    if (ret < 0) {
		return ret;
	}


	
	return ret;
}

static const struct sensor_driver_api spl06_api = {
	.sample_fetch = &spl06_sample_fetch,
	.channel_get = &spl06_channel_get,
};

/**
 * @brief initialize the sensor
 *
 * @return 0 for success
 */
static int spl06_sensor_init(const struct device *dev)
{
    int ret;
    const struct spl06_config *config = dev->config;
	struct spl06_data *data = dev->data; 
	LOG_INF("spl06 sensor init");
    ret = i2c_reg_write_byte_dt(&config->i2c, SPL06_REG_PRS_CFG, config->prs_cfg);
    if (ret < 0) {
		return ret;
	}
    ret = i2c_reg_write_byte_dt(&config->i2c, SPL06_REG_TMP_CFG, config->tmp_cfg);
    if (ret < 0) {
		return ret;
	}
    ret = i2c_reg_write_byte_dt(&config->i2c, SPL06_REG_CFG_REG, config->cfg_reg);
    if (ret < 0) {
		return ret;
	}
    ret = i2c_reg_write_byte_dt(&config->i2c, SPL06_REG_MEAS_CFG, config->meas_cfg);
    if (ret < 0) {
		return ret;
	}

	LOG_INF("spl06 sensor init ok");
    return 0;
}

static int spl06_coefficients_init(const struct device *dev)
{
	int ret;
	const struct spl06_config *config = dev->config;
	struct spl06_data *data = dev->data;
	uint8_t raw[18];
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C0     , raw);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C0C1   , raw+1);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C1     , raw+2);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C00_1  , raw+3);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C00_2  , raw+4);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C00C10 , raw+5);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C10_1  , raw+6);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C10_2  , raw+7);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C01_1  , raw+8);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C01_2  , raw+9);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C11_1  , raw+10);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C11_2  , raw+11);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C20_1  , raw+12);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C20_2  , raw+13);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C21_1  , raw+14);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C21_2  , raw+15);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C30_1  , raw+16);
    if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&config->i2c, SPL06_REG_C30_2  , raw+17);
    if (ret < 0) {
		return ret;
	}

	data->tmp_cefs[0] = (raw[0]<<4)+((raw[1]&0xF0)>>4);
	if(data->tmp_cefs[0]&0x0800) data->tmp_cefs[0] = data->tmp_cefs[0]-Total_Number_12;
	data->tmp_cefs[1] = ((raw[1]&0x0F)<<8)+raw[2];
	if(data->tmp_cefs[1]&0x0800) data->tmp_cefs[1] = data->tmp_cefs[1]-Total_Number_12;

	data->prs_cefs[0] = (raw[3]<<12)+(raw[4]<<4)+((raw[5]&0xF0)>>4);//c00
	if(data->prs_cefs[0]&0x80000) data->prs_cefs[0] = data->prs_cefs[0] - Total_Number_20;//c00
	data->prs_cefs[1] = ((raw[5]&0x0F)<<16)+ (raw[6]<<8)+ raw[7];//c10
	if(data->prs_cefs[1]&0x80000) data->prs_cefs[1] = data->prs_cefs[1] - Total_Number_20;//c10
	data->prs_cefs[2] = (raw[8]<<8)+raw[9];//c01
	if(data->prs_cefs[2]&0x8000) data->prs_cefs[2] = data->prs_cefs[2] - Total_Number_16;//c01
	data->prs_cefs[3] = (raw[10]<<8)+raw[11];//c11
	if(data->prs_cefs[3]&0x8000) data->prs_cefs[3] = data->prs_cefs[3] - Total_Number_16;//c11
	data->prs_cefs[4] = (raw[12]<<8)+raw[13];//c20
	if(data->prs_cefs[4]&0x8000) data->prs_cefs[4] = data->prs_cefs[4] - Total_Number_16;//c20
	data->prs_cefs[5] = (raw[14]<<8)+raw[15];//c21
	if(data->prs_cefs[5]&0x8000) data->prs_cefs[5] = data->prs_cefs[5] - Total_Number_16;//c21
	data->prs_cefs[6] = (raw[16]<<8)+raw[17];//c30
	if(data->prs_cefs[6]&0x8000) data->prs_cefs[6] = data->prs_cefs[6] - Total_Number_16;//c30

	switch(config->prs_cfg&0x07)
	{
	case 0:
		data->k_prs = k_SPS1;
		break;
	case 1:
		data->k_prs = k_SPS2;
		break;
	case 2:
		data->k_prs = k_SPS4;
		break;
	case 3:
		data->k_prs = k_SPS8;
		break;
	case 4:
		data->k_prs = k_SPS16;
		break;
	case 5:
		data->k_prs = k_SPS32;
		break;
	case 6:
		data->k_prs = k_SPS64;
		break;
	case 7:
		data->k_prs = k_SPS128;
		break;
	}
	// LOG_INF("prs_cfg|0x07 = %d", config->prs_cfg&0x07);
	// LOG_INF("k_prs = %d", (int)(data->k_prs));

	switch(config->tmp_cfg&0x07)
	{
	case 0:
		data->k_tmp = k_SPS1;
		break;
	case 1:
		data->k_tmp = k_SPS2;
		break;
	case 2:
		data->k_tmp = k_SPS4;
		break;
	case 3:
		data->k_tmp = k_SPS8;
		break;
	case 4:
		data->k_tmp = k_SPS16;
		break;
	case 5:
		data->k_tmp = k_SPS32;
		break;
	case 6:
		data->k_tmp = k_SPS64;
		break;
	case 7:
		data->k_tmp = k_SPS128;
		break;
	}
	// LOG_INF("tmp_cfg|0x07 = %d", config->tmp_cfg&0x07);
	// LOG_INF("k_tmp = %d", (int)(data->k_tmp));
	return 0;
}

static int spl06_init(const struct device *dev)
{
    int ret;
	const struct spl06_config *config = dev->config;
	LOG_INF("spl06 init");
	LOG_INF("addr = %d", config->i2c.addr);

    ret = device_is_ready(config->i2c.bus);
	if (ret < 0) {
		LOG_ERR("Bus device is not ready");
		return -ENODEV;
	}
	LOG_INF("Bus device is ready");

    ret = spl06_sensor_init(dev);
    if (ret < 0) {
		LOG_ERR("Couldn't init spl06");
		return ret;
	}

	ret = spl06_coefficients_init(dev);
    if (ret < 0) {
		LOG_ERR("Couldn't init spl06");
		return ret;
	}

	LOG_INF("spl06 init ok");

	return 0;
}

static int spl06_exit_sleep(const struct device *dev)
{
	int ret;
	const struct spl06_config *config = dev->config;
	ret = i2c_reg_write_byte_dt(&config->i2c, SPL06_REG_MEAS_CFG, config->meas_cfg);
    if (ret < 0) {
		return ret;
	}
	return ret;
}

static int spl06_sleep(const struct device *dev)
{
	int ret;
	const struct spl06_config *config = dev->config;
	ret = i2c_reg_write_byte_dt(&config->i2c, SPL06_REG_MEAS_CFG, 0x00);
    if (ret < 0) {
		return ret;
	}
	return ret;
}

#ifdef CONFIG_PM_DEVICE
static int spl06_pm_action(const struct device *dev,
			     enum pm_device_action action)
{
	int ret = 0;

	switch (action) {
	case PM_DEVICE_ACTION_RESUME:
		LOG_INF("resume");
		spl06_exit_sleep(dev);
		break;
	case PM_DEVICE_ACTION_SUSPEND:
		LOG_INF("suspend");
		spl06_sleep(dev);
		break;
	case PM_DEVICE_ACTION_TURN_OFF:
		LOG_INF("turn off");
		break;
	case PM_DEVICE_ACTION_TURN_ON:
		spl06_init(dev);
		LOG_INF("turn on");
		break;
	default:
		ret = -ENOTSUP;
		break;
	}

	return ret;
}
#endif /* CONFIG_PM_DEVICE */

#define SPL06_INIT(inst)								\
	static struct spl06_data spl06_data_##inst = {0};					\
											\
	const static struct spl06_config spl06_config_##inst = {\
		.i2c = I2C_DT_SPEC_INST_GET(inst),					\
        .prs_cfg = DT_INST_PROP(inst, prs_cfg), \
        .tmp_cfg = DT_INST_PROP(inst, tmp_cfg), \
        .cfg_reg = DT_INST_PROP(inst, cfg_reg), \
        .meas_cfg = DT_INST_PROP(inst, meas_cfg) \
	};										\
	\
	PM_DEVICE_DT_INST_DEFINE(inst, spl06_pm_action);	\
											\
	SENSOR_DEVICE_DT_INST_DEFINE(inst, \
								spl06_init, \
								PM_DEVICE_DT_INST_GET(inst),\
			      				&spl06_data_##inst, \
								&spl06_config_##inst, \
								POST_KERNEL,	\
			      				CONFIG_SENSOR_INIT_PRIORITY, \
								&spl06_api);		\

DT_INST_FOREACH_STATUS_OKAY(SPL06_INIT)