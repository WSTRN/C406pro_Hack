#include "qspi_flash_test.h"

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(qspi_flash_test, LOG_LEVEL_INF);

#define QSPI_FLASH_NODE DT_NODELABEL(gd25q256)
#define TEST_ERASE_SIZE 4096

static int test_flash_area(uint8_t id, uint8_t pattern)
{
	const struct flash_area *fa;
	uint8_t write_buf[32] __aligned(4);
	uint8_t read_buf[sizeof(write_buf)] __aligned(4);
	int ret;

	ret = flash_area_open(id, &fa);
	if (ret != 0) {
		LOG_ERR("open flash area %u failed: %d", id, ret);
		return ret;
	}

	for (size_t i = 0; i < sizeof(write_buf); i++) {
		write_buf[i] = (uint8_t)(pattern ^ i);
	}

	ret = flash_area_erase(fa, 0, TEST_ERASE_SIZE);
	if (ret != 0) {
		LOG_ERR("erase failed: %d", ret);
		goto out;
	}

	ret = flash_area_write(fa, 0, write_buf, sizeof(write_buf));
	if (ret != 0) {
		LOG_ERR("write failed: %d", ret);
		goto out;
	}

	memset(read_buf, 0, sizeof(read_buf));
	ret = flash_area_read(fa, 0, read_buf, sizeof(read_buf));
	if (ret != 0) {
		LOG_ERR("read failed: %d", ret);
		goto out;
	}

	if (memcmp(write_buf, read_buf, sizeof(write_buf)) != 0) {
		LOG_ERR("verify failed");
		ret = -EIO;
		goto out;
	}

	LOG_INF("QSPI flash test OK: %u bytes at %s+0x%08x",
		(uint32_t)sizeof(write_buf), fa->fa_dev->name, (uint32_t)fa->fa_off);

out:
	flash_area_close(fa);
	return ret;
}

int qspi_flash_test(void)
{
	const struct device *dev = DEVICE_DT_GET(QSPI_FLASH_NODE);
	int ret;

	if (!device_is_ready(dev)) {
		LOG_ERR("%s is not ready", dev->name);
		return -ENODEV;
	}

	ret = test_flash_area(FIXED_PARTITION_ID(qspi_test_partition), 0x5a);
	if (ret != 0) {
		return ret;
	}

	ret = test_flash_area(FIXED_PARTITION_ID(qspi_test_high_partition), 0xa5);
	if (ret != 0) {
		return ret;
	}

	ret = pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
	if (ret != 0) {
		LOG_ERR("enter DPD failed: %d", ret);
		return ret;
	}

	LOG_INF("QSPI flash entered DPD");
	k_sleep(K_MSEC(10));

	ret = pm_device_action_run(dev, PM_DEVICE_ACTION_RESUME);
	if (ret != 0) {
		LOG_ERR("exit DPD failed: %d", ret);
		return ret;
	}

	ret = test_flash_area(FIXED_PARTITION_ID(qspi_test_high_partition), 0xc3);
	if (ret != 0) {
		return ret;
	}
	LOG_INF("QSPI flash DPD wakeup OK");

	return 0;
}
