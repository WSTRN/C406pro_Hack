#include "gnss.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gnss.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

LOG_MODULE_REGISTER(gnss, LOG_LEVEL_INF);

#define GNSS_DEVICE DEVICE_DT_GET(DT_ALIAS(gnss))

static struct gnss_data latest_data;
static struct k_spinlock data_lock;

static void gnss_data_cb(const struct device *dev, const struct gnss_data *data)
{
	k_spinlock_key_t key;

	ARG_UNUSED(dev);

	key = k_spin_lock(&data_lock);
	latest_data = *data;
	k_spin_unlock(&data_lock, key);
}

GNSS_DATA_CALLBACK_DEFINE(GNSS_DEVICE, gnss_data_cb);

void gnss_init(void)
{
	int ret;

	if (!device_is_ready(GNSS_DEVICE)) {
		LOG_ERR("GNSS device not ready");
		return;
	}

	ret = pm_device_action_run(GNSS_DEVICE, PM_DEVICE_ACTION_RESUME);
	if (ret < 0 && ret != -EALREADY) {
		LOG_ERR("Failed to start GNSS device: %d", ret);
		return;
	}

	LOG_INF("GNSS device started");
}

void gnss_get_info(struct gnss_snapshot *info)
{
	struct gnss_data data;
	k_spinlock_key_t key;

	if (info == NULL) {
		return;
	}

	key = k_spin_lock(&data_lock);
	data = latest_data;
	k_spin_unlock(&data_lock, key);

	info->has_fix = data.info.fix_status != GNSS_FIX_STATUS_NO_FIX;
	info->quality = (uint8_t)data.info.fix_quality;
	if (!info->has_fix) {
		info->lat_e6 = 0;
		info->lon_e6 = 0;
		info->satellites = 0U;
		info->hdop_x10 = 0U;
		info->speed_kmh_x10 = 0U;
		info->course_deg_x10 = 0U;
		return;
	}

	info->lat_e6 = (int32_t)(data.nav_data.latitude / 1000LL);
	info->lon_e6 = (int32_t)(data.nav_data.longitude / 1000LL);
	info->satellites = (uint8_t)data.info.satellites_cnt;
	info->hdop_x10 = (uint16_t)(data.info.hdop / 100U);
	info->speed_kmh_x10 = (uint16_t)((data.nav_data.speed * 36ULL) / 1000ULL);
	info->course_deg_x10 = (uint16_t)(data.nav_data.bearing / 100U);
}
