#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(test_flash, LOG_LEVEL_INF);

#define QSPI_FLASH_NODE DT_NODELABEL(gd25q256)
#define TEST_ERASE_SIZE 4096

static int test_flash_area(const struct shell *sh, uint8_t id, uint8_t pattern)
{
	const struct flash_area *fa;
	uint8_t write_buf[32] __aligned(4);
	uint8_t read_buf[sizeof(write_buf)] __aligned(4);
	int ret;

	ret = flash_area_open(id, &fa);
	if (ret != 0) {
		shell_error(sh, "Open flash area %u failed: %d", id, ret);
		LOG_ERR("open flash area %u failed: %d", id, ret);
		return ret;
	}

	shell_print(sh, "Device: %s", fa->fa_dev->name);
	shell_print(sh, "Partition: offset=0x%08x, size=%u bytes",
		    (uint32_t)fa->fa_off, (uint32_t)fa->fa_size);

	for (size_t i = 0; i < sizeof(write_buf); i++) {
		write_buf[i] = (uint8_t)(pattern ^ i);
	}

	shell_print(sh, "[1/4] Erasing %u bytes...", TEST_ERASE_SIZE);
	ret = flash_area_erase(fa, 0, TEST_ERASE_SIZE);
	if (ret != 0) {
		shell_error(sh, "Erase failed: %d", ret);
		LOG_ERR("erase failed: %d", ret);
		goto out;
	}

	shell_print(sh, "[2/4] Writing %u bytes with pattern 0x%02x...",
		    (uint32_t)sizeof(write_buf), pattern);
	ret = flash_area_write(fa, 0, write_buf, sizeof(write_buf));
	if (ret != 0) {
		shell_error(sh, "Write failed: %d", ret);
		LOG_ERR("write failed: %d", ret);
		goto out;
	}

	shell_print(sh, "[3/4] Reading %u bytes...", (uint32_t)sizeof(read_buf));
	memset(read_buf, 0, sizeof(read_buf));
	ret = flash_area_read(fa, 0, read_buf, sizeof(read_buf));
	if (ret != 0) {
		shell_error(sh, "Read failed: %d", ret);
		LOG_ERR("read failed: %d", ret);
		goto out;
	}

	shell_print(sh, "[4/4] Verifying data...");
	if (memcmp(write_buf, read_buf, sizeof(write_buf)) != 0) {
		shell_error(sh, "Verification failed");
		LOG_ERR("verify failed");
		ret = -EIO;
		goto out;
	}

	shell_print(sh, "QSPI flash test passed");
	LOG_INF("QSPI flash test OK: %u bytes at %s+0x%08x",
		(uint32_t)sizeof(write_buf), fa->fa_dev->name, (uint32_t)fa->fa_off);

out:
	flash_area_close(fa);
	return ret;
}

static int qspi_flash_test(const struct shell *sh)
{
	const struct device *dev = DEVICE_DT_GET(QSPI_FLASH_NODE);

	if (!device_is_ready(dev)) {
		shell_error(sh, "QSPI device %s is not ready", dev->name);
		LOG_ERR("%s is not ready", dev->name);
		return -ENODEV;
	}

	shell_warn(sh, "The reserved 64 KB test partition will be modified");
	return test_flash_area(sh, FIXED_PARTITION_ID(reserved_partition), 0x5a);
}

static int cmd_test_flash(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Starting QSPI flash test");
	return qspi_flash_test(sh);
}

SHELL_CMD_REGISTER(test_flash, NULL, "Test QSPI erase/write/read", cmd_test_flash);
