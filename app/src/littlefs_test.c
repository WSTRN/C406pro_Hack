#include "littlefs_test.h"

#include <errno.h>
#include <string.h>

#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

LOG_MODULE_REGISTER(littlefs_test, LOG_LEVEL_INF);

#define LITTLEFS_PARTITION littlefs_partition
#define LITTLEFS_PARTITION_ID FIXED_PARTITION_ID(LITTLEFS_PARTITION)
#define LITTLEFS_MOUNT_POINT "/lfs"
#define LITTLEFS_TEST_FILE LITTLEFS_MOUNT_POINT "/qspi.txt"

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(littlefs_storage);

static struct fs_mount_t littlefs_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &littlefs_storage,
	.storage_dev = (void *)LITTLEFS_PARTITION_ID,
	.mnt_point = LITTLEFS_MOUNT_POINT,
};

static int write_test_file(void)
{
	struct fs_file_t file;
	const char test_data[] = "C406pro littlefs qspi test";
	ssize_t written;
	int ret;

	fs_file_t_init(&file);

	ret = fs_unlink(LITTLEFS_TEST_FILE);
	if (ret != 0 && ret != -ENOENT) {
		LOG_ERR("unlink %s failed: %d", LITTLEFS_TEST_FILE, ret);
		return ret;
	}

	ret = fs_open(&file, LITTLEFS_TEST_FILE, FS_O_CREATE | FS_O_WRITE);
	if (ret != 0) {
		LOG_ERR("open %s for write failed: %d", LITTLEFS_TEST_FILE, ret);
		return ret;
	}

	written = fs_write(&file, test_data, sizeof(test_data));
	if (written < 0) {
		LOG_ERR("write %s failed: %d", LITTLEFS_TEST_FILE, (int)written);
		ret = (int)written;
	} else if (written != sizeof(test_data)) {
		LOG_ERR("short write %s: %d/%u", LITTLEFS_TEST_FILE,
			(int)written, (uint32_t)sizeof(test_data));
		ret = -EIO;
	} else {
		ret = fs_sync(&file);
		if (ret != 0) {
			LOG_ERR("sync %s failed: %d", LITTLEFS_TEST_FILE, ret);
		}
	}

	(void)fs_close(&file);
	return ret;
}

static int read_test_file(void)
{
	struct fs_file_t file;
	const char test_data[] = "C406pro littlefs qspi test";
	char read_buf[sizeof(test_data)];
	ssize_t bytes_read;
	int ret;

	fs_file_t_init(&file);

	ret = fs_open(&file, LITTLEFS_TEST_FILE, FS_O_READ);
	if (ret != 0) {
		LOG_ERR("open %s for read failed: %d", LITTLEFS_TEST_FILE, ret);
		return ret;
	}

	memset(read_buf, 0, sizeof(read_buf));
	bytes_read = fs_read(&file, read_buf, sizeof(read_buf));
	if (bytes_read < 0) {
		LOG_ERR("read %s failed: %d", LITTLEFS_TEST_FILE, (int)bytes_read);
		ret = (int)bytes_read;
	} else if (bytes_read != sizeof(test_data)) {
		LOG_ERR("short read %s: %d/%u", LITTLEFS_TEST_FILE,
			(int)bytes_read, (uint32_t)sizeof(test_data));
		ret = -EIO;
	} else if (memcmp(test_data, read_buf, sizeof(test_data)) != 0) {
		LOG_ERR("verify %s failed", LITTLEFS_TEST_FILE);
		ret = -EIO;
	} else {
		ret = 0;
	}

	(void)fs_close(&file);
	return ret;
}

int littlefs_test(void)
{
	struct fs_statvfs stat;
	int ret;

	ret = fs_mount(&littlefs_mnt);
	if (ret != 0) {
		LOG_ERR("mount %s failed: %d", LITTLEFS_MOUNT_POINT, ret);
		return ret;
	}

	LOG_INF("mounted %s on qspi littlefs partition", LITTLEFS_MOUNT_POINT);

	ret = write_test_file();
	if (ret != 0) {
		goto out;
	}

	ret = read_test_file();
	if (ret != 0) {
		goto out;
	}

	ret = fs_statvfs(LITTLEFS_MOUNT_POINT, &stat);
	if (ret == 0) {
		LOG_INF("littlefs space: block size %lu, total %lu, free %lu",
			(unsigned long)stat.f_bsize,
			(unsigned long)stat.f_blocks,
			(unsigned long)stat.f_bfree);
	} else {
		LOG_WRN("statvfs %s failed: %d", LITTLEFS_MOUNT_POINT, ret);
	}

	LOG_INF("littlefs test OK: %s", LITTLEFS_TEST_FILE);

out:
	(void)fs_unmount(&littlefs_mnt);
	return ret;
}
