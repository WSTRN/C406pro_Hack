#include <errno.h>
#include <string.h>

#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(test_fs, LOG_LEVEL_INF);

#define LITTLEFS_PARTITION storage_partition
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

static int write_test_file(const struct shell *sh)
{
	struct fs_file_t file;
	const char test_data[] = "C406pro littlefs qspi test";
	ssize_t written;
	int ret;

	fs_file_t_init(&file);

	shell_print(sh, "[2/6] Opening %s for writing...", LITTLEFS_TEST_FILE);
	ret = fs_open(&file, LITTLEFS_TEST_FILE,
		      FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
	if (ret != 0) {
		shell_error(sh, "Open for write failed: %d", ret);
		LOG_ERR("open %s for write failed: %d", LITTLEFS_TEST_FILE, ret);
		return ret;
	}

	shell_print(sh, "[3/6] Writing and syncing %u bytes...",
		    (uint32_t)sizeof(test_data));
	written = fs_write(&file, test_data, sizeof(test_data));
	if (written < 0) {
		shell_error(sh, "Write failed: %d", (int)written);
		LOG_ERR("write %s failed: %d", LITTLEFS_TEST_FILE, (int)written);
		ret = (int)written;
	} else if (written != sizeof(test_data)) {
		shell_error(sh, "Short write: %d/%u", (int)written,
			    (uint32_t)sizeof(test_data));
		LOG_ERR("short write %s: %d/%u", LITTLEFS_TEST_FILE,
			(int)written, (uint32_t)sizeof(test_data));
		ret = -EIO;
	} else {
		ret = fs_sync(&file);
		if (ret != 0) {
			shell_error(sh, "Sync failed: %d", ret);
			LOG_ERR("sync %s failed: %d", LITTLEFS_TEST_FILE, ret);
		}
	}

	(void)fs_close(&file);
	return ret;
}

static int read_test_file(const struct shell *sh)
{
	struct fs_file_t file;
	const char test_data[] = "C406pro littlefs qspi test";
	char read_buf[sizeof(test_data)];
	ssize_t bytes_read;
	int ret;

	fs_file_t_init(&file);

	shell_print(sh, "[4/6] Reading %s...", LITTLEFS_TEST_FILE);
	ret = fs_open(&file, LITTLEFS_TEST_FILE, FS_O_READ);
	if (ret != 0) {
		shell_error(sh, "Open for read failed: %d", ret);
		LOG_ERR("open %s for read failed: %d", LITTLEFS_TEST_FILE, ret);
		return ret;
	}

	memset(read_buf, 0, sizeof(read_buf));
	bytes_read = fs_read(&file, read_buf, sizeof(read_buf));
	if (bytes_read < 0) {
		shell_error(sh, "Read failed: %d", (int)bytes_read);
		LOG_ERR("read %s failed: %d", LITTLEFS_TEST_FILE, (int)bytes_read);
		ret = (int)bytes_read;
	} else if (bytes_read != sizeof(test_data)) {
		shell_error(sh, "Short read: %d/%u", (int)bytes_read,
			    (uint32_t)sizeof(test_data));
		LOG_ERR("short read %s: %d/%u", LITTLEFS_TEST_FILE,
			(int)bytes_read, (uint32_t)sizeof(test_data));
		ret = -EIO;
	} else if (memcmp(test_data, read_buf, sizeof(test_data)) != 0) {
		shell_error(sh, "Data verification failed");
		LOG_ERR("verify %s failed", LITTLEFS_TEST_FILE);
		ret = -EIO;
	} else {
		shell_print(sh, "[5/6] Data verification passed");
		ret = 0;
	}

	(void)fs_close(&file);
	return ret;
}

static int littlefs_test(const struct shell *sh)
{
	struct fs_statvfs stat;
	int ret;

	shell_print(sh, "Partition: %s, mount point: %s",
		    STRINGIFY(LITTLEFS_PARTITION), LITTLEFS_MOUNT_POINT);
	shell_print(sh, "[1/6] Mounting LittleFS...");
	ret = fs_mount(&littlefs_mnt);
	if (ret != 0) {
		shell_error(sh, "Mount failed: %d", ret);
		LOG_ERR("mount %s failed: %d", LITTLEFS_MOUNT_POINT, ret);
		return ret;
	}

	LOG_INF("mounted %s on qspi littlefs partition", LITTLEFS_MOUNT_POINT);
	shell_print(sh, "Mounted successfully");

	ret = write_test_file(sh);
	if (ret != 0) {
		goto out;
	}

	ret = read_test_file(sh);
	if (ret != 0) {
		goto out;
	}

	ret = fs_statvfs(LITTLEFS_MOUNT_POINT, &stat);
	if (ret == 0) {
		shell_print(sh, "Capacity: block_size=%lu, total_blocks=%lu, free_blocks=%lu",
			    (unsigned long)stat.f_bsize,
			    (unsigned long)stat.f_blocks,
			    (unsigned long)stat.f_bfree);
		LOG_INF("littlefs space: block size %lu, total %lu, free %lu",
			(unsigned long)stat.f_bsize,
			(unsigned long)stat.f_blocks,
			(unsigned long)stat.f_bfree);
	} else {
		shell_warn(sh, "Capacity query failed: %d", ret);
		LOG_WRN("statvfs %s failed: %d", LITTLEFS_MOUNT_POINT, ret);
	}

	LOG_INF("littlefs test OK: %s", LITTLEFS_TEST_FILE);
	ret = 0;

out:
	shell_print(sh, "[6/6] Unmounting LittleFS...");
	{
		int unmount_ret = fs_unmount(&littlefs_mnt);

		if (unmount_ret != 0) {
			shell_error(sh, "Unmount failed: %d", unmount_ret);
			if (ret == 0) {
				ret = unmount_ret;
			}
		}
	}
	if (ret == 0) {
		shell_print(sh, "LittleFS test passed");
	}
	return ret;
}

static int cmd_test_fs(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Starting LittleFS test");
	return littlefs_test(sh);
}

SHELL_CMD_REGISTER(test_fs, NULL, "Test LittleFS mount/write/read", cmd_test_fs);
