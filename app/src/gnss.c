#include "gnss.h"

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gnss.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(gnss, LOG_LEVEL_INF);

#define GNSS_DEVICE DEVICE_DT_GET(DT_ALIAS(gnss))

static struct gnss_data latest_data;
static struct gnss_satellite satellite_buffers[2][GNSS_SATELLITE_CACHE_SIZE];
static size_t satellite_counts[2];
static uint8_t building_buffer;
static uint8_t complete_buffer;
static enum gnss_system first_system;
static bool building_started;
static bool complete_buffer_valid;
static bool data_valid;
static struct k_spinlock data_lock;

static void gnss_data_cb(const struct device *dev, const struct gnss_data *data)
{
    k_spinlock_key_t key;

    ARG_UNUSED(dev);

    key = k_spin_lock(&data_lock);
    latest_data = *data;
    data_valid = true;
    k_spin_unlock(&data_lock, key);
}

GNSS_DATA_CALLBACK_DEFINE(GNSS_DEVICE, gnss_data_cb);

static void gnss_satellites_cb(const struct device *dev, const struct gnss_satellite *satellites, uint16_t size)
{
    k_spinlock_key_t key;
    size_t count;

    ARG_UNUSED(dev);

    if (size == 0U)
    {
        return;
    }

    key = k_spin_lock(&data_lock);

    if (building_started && satellites[0].system == first_system)
    {
        complete_buffer = building_buffer;
        complete_buffer_valid = true;

        building_buffer ^= 1U;
        satellite_counts[building_buffer] = 0U;
        building_started = false;
    }

    if (!building_started)
    {
        first_system = satellites[0].system;
        building_started = true;
    }

    count = MIN((size_t)size, ARRAY_SIZE(satellite_buffers[building_buffer]) - satellite_counts[building_buffer]);
    memcpy(&satellite_buffers[building_buffer][satellite_counts[building_buffer]], satellites,
           count * sizeof(*satellites));

    satellite_counts[building_buffer] += count;
    k_spin_unlock(&data_lock, key);
}

GNSS_SATELLITES_CALLBACK_DEFINE(GNSS_DEVICE, gnss_satellites_cb);

void gnss_init(void)
{
    int ret;

    if (!device_is_ready(GNSS_DEVICE))
    {
        LOG_ERR("GNSS device not ready");
        return;
    }

    ret = pm_device_action_run(GNSS_DEVICE, PM_DEVICE_ACTION_RESUME);
    if (ret < 0 && ret != -EALREADY)
    {
        LOG_ERR("Failed to start GNSS device: %d", ret);
        return;
    }

    LOG_INF("GNSS device started");
}

bool gnss_get_info(struct gnss_data *data)
{
    bool valid;
    k_spinlock_key_t key;

    if (data == NULL)
    {
        return false;
    }

    key = k_spin_lock(&data_lock);
    *data = latest_data;
    valid = data_valid;
    k_spin_unlock(&data_lock, key);

    return valid;
}

size_t gnss_get_satellites(struct gnss_satellite *satellites, size_t capacity)
{
    size_t count;
    k_spinlock_key_t key;

    if (satellites == NULL || capacity == 0U)
    {
        return 0U;
    }

    key = k_spin_lock(&data_lock);
    if (!complete_buffer_valid)
    {
        count = 0U;
    }
    else
    {
        count = MIN(capacity, satellite_counts[complete_buffer]);
        memcpy(satellites, satellite_buffers[complete_buffer], count * sizeof(*satellites));
    }
    k_spin_unlock(&data_lock, key);

    return count;
}
