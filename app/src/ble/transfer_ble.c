#include <errno.h>
#include <string.h>

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include "ble/ble.h"

LOG_MODULE_REGISTER(transfer_ble, LOG_LEVEL_INF);

/* 8c7a0001-5b7d-4f6c-9a2e-3d4b6f8a1000 */
#define BT_UUID_TRANSFER_SERVICE BT_UUID_DECLARE_128( \
	BT_UUID_128_ENCODE(0x8c7a0001, 0x5b7d, 0x4f6c, 0x9a2e, 0x3d4b6f8a1000))
#define BT_UUID_TRANSFER_CONTROL BT_UUID_DECLARE_128( \
	BT_UUID_128_ENCODE(0x8c7a0002, 0x5b7d, 0x4f6c, 0x9a2e, 0x3d4b6f8a1000))
#define BT_UUID_TRANSFER_DATA BT_UUID_DECLARE_128( \
	BT_UUID_128_ENCODE(0x8c7a0003, 0x5b7d, 0x4f6c, 0x9a2e, 0x3d4b6f8a1000))

#define TRANSFER_ROOT "/lfs"
#define TRANSFER_NAME_MAX 63
#define TRANSFER_PACKET_MAX 244
#define TRANSFER_STACK_SIZE 2304
#define TRANSFER_PRIORITY 7

enum transfer_cmd {
	CMD_LIST = 1,
	CMD_UPLOAD_BEGIN = 2,
	CMD_UPLOAD_END = 3,
	CMD_DOWNLOAD = 4,
	CMD_DELETE = 5,
	CMD_CANCEL = 6,
};

enum transfer_rsp {
	RSP_LIST_ITEM = 0x81,
	RSP_LIST_DONE = 0x82,
	RSP_UPLOAD_READY = 0x83,
	RSP_UPLOAD_DONE = 0x84,
	RSP_DOWNLOAD_INFO = 0x85,
	RSP_DOWNLOAD_DONE = 0x86,
	RSP_DELETE_DONE = 0x87,
	RSP_UPLOAD_CHUNK = 0x88,
	RSP_CANCEL_DONE = 0x89,
};

enum transfer_event_type {
	EVENT_CONTROL,
	EVENT_DATA,
	EVENT_DISCONNECT,
};

struct transfer_event {
	uint8_t type;
	uint16_t len;
	uint8_t data[TRANSFER_PACKET_MAX];
};

static struct fs_file_t upload_file;
static bool upload_active;
static uint32_t upload_size;
static uint32_t upload_offset;
static char upload_path[sizeof(TRANSFER_ROOT) + 1 + TRANSFER_NAME_MAX + 1];
static bool ctrl_notify;
static bool data_notify;
static atomic_t cancel_requested;
K_MSGQ_DEFINE(event_queue, sizeof(struct transfer_event), 4, 4);

static void ctrl_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	ctrl_notify = value == BT_GATT_CCC_NOTIFY;
}

static void data_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	data_notify = value == BT_GATT_CCC_NOTIFY;
}

static ssize_t ctrl_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  const void *buf, uint16_t len, uint16_t offset, uint8_t flags);
static ssize_t data_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  const void *buf, uint16_t len, uint16_t offset, uint8_t flags);

BT_GATT_SERVICE_DEFINE(transfer_service,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_TRANSFER_SERVICE),
	BT_GATT_CHARACTERISTIC(BT_UUID_TRANSFER_CONTROL,
		BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_WRITE, NULL, ctrl_write, NULL),
	BT_GATT_CCC(ctrl_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_TRANSFER_DATA,
		BT_GATT_CHRC_WRITE_WITHOUT_RESP | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_WRITE, NULL, data_write, NULL),
	BT_GATT_CCC(data_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));

static int send_notify(const struct bt_gatt_attr *attr, const void *data, uint16_t len)
{
	struct bt_conn *conn = ble_conn();

	return conn ? bt_gatt_notify(conn, attr, data, len) : -ENOTCONN;
}

static int send_ctrl(uint8_t response, int status, const void *payload, size_t payload_len)
{
	uint8_t packet[2 + 4 + TRANSFER_NAME_MAX];

	if (!ctrl_notify || payload_len > sizeof(packet) - 2) {
		return -EACCES;
	}
	packet[0] = response;
	packet[1] = status < 0 ? (uint8_t)MIN(-status, UINT8_MAX) : 0;
	if (payload_len) {
		memcpy(&packet[2], payload, payload_len);
	}
	return send_notify(&transfer_service.attrs[2], packet, payload_len + 2);
}

static int send_ctrl_retry(uint8_t response, int status,
			   const void *payload, size_t payload_len)
{
	int err;

	for (;;) {
		err = send_ctrl(response, status, payload, payload_len);
		if (err != -ENOMEM && err != -EAGAIN) {
			return err;
		}
		k_msleep(2);
	}
}

static int make_path(const uint8_t *name, size_t name_len, char *path, size_t path_size)
{
	if (!name_len || name_len > TRANSFER_NAME_MAX ||
	    memchr(name, '/', name_len) || memchr(name, '\\', name_len)) {
		return -EINVAL;
	}
	if ((name_len == 1 && name[0] == '.') ||
	    (name_len == 2 && name[0] == '.' && name[1] == '.')) {
		return -EINVAL;
	}
	snprintk(path, path_size, TRANSFER_ROOT "/%.*s", (int)name_len, name);
	return 0;
}

static int parse_path(const uint8_t *buf, uint16_t len, uint16_t start,
		      char *path, size_t path_size)
{
	return len > start ? make_path(&buf[start], len - start, path, path_size) : -EINVAL;
}

static void list_files(void)
{
	struct fs_dir_t dir;
	struct fs_dirent entry;
	uint8_t payload[4 + TRANSFER_NAME_MAX];
	int err;

	if (upload_active) {
		(void)send_ctrl_retry(RSP_LIST_DONE, -EBUSY, NULL, 0);
		return;
	}
	fs_dir_t_init(&dir);
	err = fs_opendir(&dir, TRANSFER_ROOT);
	while (!err) {
		err = fs_readdir(&dir, &entry);
		if (err || !entry.name[0]) {
			break;
		}
		if (entry.type != FS_DIR_ENTRY_FILE) {
			continue;
		}
		sys_put_le32(entry.size, payload);
		size_t name_len = MIN(strlen(entry.name), TRANSFER_NAME_MAX);
		memcpy(&payload[4], entry.name, name_len);
		if (send_ctrl_retry(RSP_LIST_ITEM, 0, payload, 4 + name_len)) {
			err = -EIO;
			break;
		}
	}
	(void)fs_closedir(&dir);
	(void)send_ctrl_retry(RSP_LIST_DONE, err, NULL, 0);
}

static void upload_begin(const uint8_t *buf, uint16_t len)
{
	int err;

	if (len < 6) {
		err = -EINVAL;
	} else if (upload_active) {
		err = -EBUSY;
	} else {
		err = make_path(&buf[5], len - 5, upload_path, sizeof(upload_path));
		if (!err) {
			fs_file_t_init(&upload_file);
			err = fs_open(&upload_file, upload_path,
				      FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
			if (!err) {
				upload_active = true;
				upload_size = sys_get_le32(&buf[1]);
				upload_offset = 0;
			}
		}
	}
	(void)send_ctrl_retry(RSP_UPLOAD_READY, err, NULL, 0);
}

static void upload_data(const uint8_t *buf, uint16_t len)
{
	uint8_t payload[4];
	ssize_t bytes_written;
	int err = 0;

	if (!upload_active || upload_offset + len > upload_size) {
		err = -EINVAL;
	} else {
		bytes_written = fs_write(&upload_file, buf, len);
		if (bytes_written != len) {
			err = bytes_written < 0 ? (int)bytes_written : -EIO;
		} else {
			upload_offset += bytes_written;
		}
	}
	sys_put_le32(upload_offset, payload);
	(void)send_ctrl_retry(RSP_UPLOAD_CHUNK, err, payload, sizeof(payload));
}

static void upload_end(void)
{
	uint8_t payload[4];
	int err;

	if (!upload_active) {
		err = -EINVAL;
	} else {
		err = fs_sync(&upload_file);
		if (!err && upload_offset != upload_size) {
			err = -EMSGSIZE;
		}
		(void)fs_close(&upload_file);
		upload_active = false;
		upload_path[0] = '\0';
	}
	sys_put_le32(upload_offset, payload);
	(void)send_ctrl_retry(RSP_UPLOAD_DONE, err, payload, sizeof(payload));
}

static void delete_file(const uint8_t *buf, uint16_t len)
{
	char path[sizeof(TRANSFER_ROOT) + 1 + TRANSFER_NAME_MAX + 1];
	int err = upload_active ? -EBUSY : parse_path(buf, len, 1, path, sizeof(path));

	if (!err) {
		err = fs_unlink(path);
	}
	(void)send_ctrl_retry(RSP_DELETE_DONE, err, NULL, 0);
}

static int send_data_retry(const uint8_t *data, uint16_t len)
{
	int err;

	for (;;) {
		if (atomic_get(&cancel_requested)) {
			return -ECANCELED;
		}
		if (!data_notify) {
			return -EACCES;
		}
		err = send_notify(&transfer_service.attrs[5], data, len);
		if (err != -ENOMEM && err != -EAGAIN) {
			return err;
		}
		k_msleep(2);
	}
}

static void cancel_transfer(void)
{
	if (upload_active) {
		(void)fs_close(&upload_file);
		upload_active = false;
		if (upload_path[0]) {
			(void)fs_unlink(upload_path);
			upload_path[0] = '\0';
		}
	}
	atomic_clear(&cancel_requested);
	(void)send_ctrl_retry(RSP_CANCEL_DONE, 0, NULL, 0);
}

static void download_file(const uint8_t *cmd, uint16_t cmd_len)
{
	struct fs_file_t file;
	struct fs_dirent entry;
	char path[sizeof(TRANSFER_ROOT) + 1 + TRANSFER_NAME_MAX + 1];
	uint8_t size_buf[4];
	uint8_t buf[TRANSFER_PACKET_MAX];
	ssize_t bytes_read;
	int err = upload_active ? -EBUSY : parse_path(cmd, cmd_len, 1, path, sizeof(path));

	fs_file_t_init(&file);
	if (!err) {
		err = fs_stat(path, &entry);
	}
	if (!err) {
		err = fs_open(&file, path, FS_O_READ);
	}
	if (!err) {
		sys_put_le32(entry.size, size_buf);
		err = send_ctrl_retry(RSP_DOWNLOAD_INFO, 0, size_buf, sizeof(size_buf));
	} else {
		(void)send_ctrl_retry(RSP_DOWNLOAD_INFO, err, NULL, 0);
		return;
	}

	while (!err) {
		if (atomic_get(&cancel_requested)) {
			err = -ECANCELED;
			break;
		}
		bytes_read = fs_read(&file, buf, sizeof(buf));
		if (bytes_read <= 0) {
			err = bytes_read < 0 ? (int)bytes_read : 0;
			break;
		}
		struct bt_conn *conn = ble_conn();
		if (!conn) {
			err = -ENOTCONN;
			break;
		}
		uint16_t chunk_size = MAX(1, bt_gatt_get_mtu(conn) - 3);
		for (size_t offset = 0; offset < bytes_read && !err; offset += chunk_size) {
			err = send_data_retry(&buf[offset], MIN(chunk_size, bytes_read - offset));
		}
	}
	(void)fs_close(&file);
	(void)send_ctrl_retry(RSP_DOWNLOAD_DONE, err, NULL, 0);
}

static int queue_event(uint8_t type, const void *buf, uint16_t len)
{
	struct transfer_event event = {
		.type = type,
		.len = len,
	};

	if (len > sizeof(event.data)) {
		return -EMSGSIZE;
	}
	if (len) {
		memcpy(event.data, buf, len);
	}
	return k_msgq_put(&event_queue, &event, K_NO_WAIT);
}

static ssize_t ctrl_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	const uint8_t *cmd = buf;

	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(flags);
	if (offset || !len) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	if (cmd[0] < CMD_LIST || cmd[0] > CMD_CANCEL) {
		return BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED);
	}
	if (cmd[0] == CMD_CANCEL) {
		atomic_set(&cancel_requested, 1);
	}
	if (queue_event(EVENT_CONTROL, buf, len)) {
		if (cmd[0] == CMD_CANCEL) {
			atomic_clear(&cancel_requested);
		}
		return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
	}
	return len;
}

static ssize_t data_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(flags);
	if (offset) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	return queue_event(EVENT_DATA, buf, len) ?
		BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES) : len;
}

static void transfer_worker(void)
{
	struct transfer_event event;

	for (;;) {
		k_msgq_get(&event_queue, &event, K_FOREVER);
		if (event.type == EVENT_DISCONNECT) {
			if (upload_active) {
				(void)fs_close(&upload_file);
				upload_active = false;
				upload_path[0] = '\0';
			}
			atomic_clear(&cancel_requested);
			continue;
		}
		if (event.type == EVENT_DATA) {
			upload_data(event.data, event.len);
			continue;
		}
		switch (event.data[0]) {
		case CMD_LIST:
			list_files();
			break;
		case CMD_UPLOAD_BEGIN:
			upload_begin(event.data, event.len);
			break;
		case CMD_UPLOAD_END:
			upload_end();
			break;
		case CMD_DOWNLOAD:
			download_file(event.data, event.len);
			break;
		case CMD_DELETE:
			delete_file(event.data, event.len);
			break;
		case CMD_CANCEL:
			cancel_transfer();
			break;
		}
	}
}

K_THREAD_DEFINE(transfer_worker_tid, TRANSFER_STACK_SIZE, transfer_worker,
		NULL, NULL, NULL, TRANSFER_PRIORITY, 0, 0);

static void transfer_disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(reason);
	(void)queue_event(EVENT_DISCONNECT, NULL, 0);
}

BT_CONN_CB_DEFINE(transfer_conn_callbacks) = {
	.disconnected = transfer_disconnected,
};
