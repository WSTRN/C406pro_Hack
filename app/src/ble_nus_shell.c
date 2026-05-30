#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(ble_nus_shell, LOG_LEVEL_INF);

#define BLE_NUS_SHELL_PROMPT "c406pro> "
#define BLE_NUS_SHELL_RX_RING_BUFFER_SIZE 256
#define BLE_NUS_SHELL_LOG_QUEUE_SIZE 512
#define BLE_NUS_SHELL_LOG_QUEUE_TIMEOUT 100

#define BT_UUID_NUS_VAL \
	BT_UUID_128_ENCODE(0x6e400001, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)
#define BT_UUID_NUS_RX_VAL \
	BT_UUID_128_ENCODE(0x6e400002, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)
#define BT_UUID_NUS_TX_VAL \
	BT_UUID_128_ENCODE(0x6e400003, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)

static struct bt_uuid_128 nus_uuid = BT_UUID_INIT_128(BT_UUID_NUS_VAL);
static struct bt_uuid_128 nus_rx_uuid = BT_UUID_INIT_128(BT_UUID_NUS_RX_VAL);
static struct bt_uuid_128 nus_tx_uuid = BT_UUID_INIT_128(BT_UUID_NUS_TX_VAL);

struct bt_nus_shell_transport {
	shell_transport_handler_t handler;
	void *context;
	struct ring_buf rx_ringbuf;
	uint8_t rx_buf[BLE_NUS_SHELL_RX_RING_BUFFER_SIZE];
	struct k_mutex tx_lock;
	bool initialized;
	bool blocking_tx;
};

static struct bt_conn *active_conn;
static bool notify_enabled;
static struct bt_nus_shell_transport transport_ctx;
static const uint8_t ad_flags[] = {
	BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR,
};

static ssize_t nus_rx_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			    const void *buf, uint16_t len, uint16_t offset,
			    uint8_t flags);
static void nus_tx_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value);

BT_GATT_SERVICE_DEFINE(nus_svc,
	BT_GATT_PRIMARY_SERVICE(&nus_uuid),
	BT_GATT_CHARACTERISTIC(&nus_rx_uuid.uuid,
			       BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_WRITE, NULL, nus_rx_write, NULL),
	BT_GATT_CHARACTERISTIC(&nus_tx_uuid.uuid, BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_NONE, NULL, NULL, NULL),
	BT_GATT_CCC(nus_tx_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_WRN("Bluetooth connection failed: %u", err);
		return;
	}

	if (!active_conn) {
		active_conn = bt_conn_ref(conn);
	}

	LOG_INF("Bluetooth connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	if (active_conn) {
		bt_conn_unref(active_conn);
		active_conn = NULL;
	}

	notify_enabled = false;
	LOG_INF("Bluetooth disconnected: %u", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void nus_tx_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	notify_enabled = (value == BT_GATT_CCC_NOTIFY);
}

static ssize_t nus_rx_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			    const void *buf, uint16_t len, uint16_t offset,
			    uint8_t flags)
{
	uint32_t written;

	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(flags);

	if (offset != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	written = ring_buf_put(&transport_ctx.rx_ringbuf, buf, len);
	if (written != len) {
		LOG_WRN("BLE NUS shell RX ring buffer full");
	}

	if (written && transport_ctx.handler) {
		transport_ctx.handler(SHELL_TRANSPORT_EVT_RX_RDY, transport_ctx.context);
	}

	return len;
}

static int transport_init(const struct shell_transport *transport,
			  const void *config,
			  shell_transport_handler_t evt_handler,
			  void *context)
{
	struct bt_nus_shell_transport *bt_nus = transport->ctx;

	ARG_UNUSED(config);

	if (bt_nus->initialized) {
		return -EINVAL;
	}

	bt_nus->handler = evt_handler;
	bt_nus->context = context;
	ring_buf_init(&bt_nus->rx_ringbuf, sizeof(bt_nus->rx_buf), bt_nus->rx_buf);
	k_mutex_init(&bt_nus->tx_lock);
	bt_nus->initialized = true;

	return 0;
}

static int transport_uninit(const struct shell_transport *transport)
{
	struct bt_nus_shell_transport *bt_nus = transport->ctx;

	if (!bt_nus->initialized) {
		return -ENODEV;
	}

	bt_nus->initialized = false;
	return 0;
}

static int transport_enable(const struct shell_transport *transport, bool blocking_tx)
{
	struct bt_nus_shell_transport *bt_nus = transport->ctx;

	if (!bt_nus->initialized) {
		return -ENODEV;
	}

	bt_nus->blocking_tx = blocking_tx;
	return 0;
}

static int transport_write(const struct shell_transport *transport,
			   const void *data, size_t length, size_t *cnt)
{
	struct bt_nus_shell_transport *bt_nus = transport->ctx;
	const uint8_t *bytes = data;
	size_t mtu_payload;
	size_t sent = 0;
	int err = 0;

	if (!bt_nus->initialized) {
		*cnt = 0;
		return -ENODEV;
	}

	if (!active_conn || !notify_enabled) {
		*cnt = length;
		return 0;
	}

	k_mutex_lock(&bt_nus->tx_lock, K_FOREVER);

	mtu_payload = MAX(1, bt_gatt_get_mtu(active_conn) - 3);

	while (sent < length) {
		size_t chunk = MIN(length - sent, mtu_payload);

		err = bt_gatt_notify(active_conn, &nus_svc.attrs[4],
				     &bytes[sent], chunk);
		if (err) {
			break;
		}

		sent += chunk;
	}

	k_mutex_unlock(&bt_nus->tx_lock);

	*cnt = sent;

	if (bt_nus->handler) {
		bt_nus->handler(SHELL_TRANSPORT_EVT_TX_RDY, bt_nus->context);
	}

	return err;
}

static int transport_read(const struct shell_transport *transport,
			  void *data, size_t length, size_t *cnt)
{
	struct bt_nus_shell_transport *bt_nus = transport->ctx;

	if (!bt_nus->initialized) {
		*cnt = 0;
		return -ENODEV;
	}

	*cnt = ring_buf_get(&bt_nus->rx_ringbuf, data, length);
	return 0;
}

static const struct shell_transport_api bt_nus_shell_transport_api = {
	.init = transport_init,
	.uninit = transport_uninit,
	.enable = transport_enable,
	.write = transport_write,
	.read = transport_read,
};

static const struct shell_transport shell_transport_bt_nus = {
	.api = &bt_nus_shell_transport_api,
	.ctx = &transport_ctx,
};

SHELL_DEFINE(shell_bt_nus, BLE_NUS_SHELL_PROMPT, &shell_transport_bt_nus,
	     BLE_NUS_SHELL_LOG_QUEUE_SIZE,
	     BLE_NUS_SHELL_LOG_QUEUE_TIMEOUT,
	     SHELL_FLAG_OLF_CRLF);

static int enable_shell_bt_nus(void)
{
	struct shell_backend_config_flags cfg_flags =
		SHELL_DEFAULT_BACKEND_CONFIG_FLAGS;

	shell_init(&shell_bt_nus, NULL, cfg_flags, false, 0);
	return 0;
}

SYS_INIT(enable_shell_bt_nus, POST_KERNEL, 0);

int ble_nus_shell_start(void)
{
	static const struct bt_data ad[] = {
		BT_DATA(BT_DATA_FLAGS, ad_flags, sizeof(ad_flags)),
		BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
			sizeof(CONFIG_BT_DEVICE_NAME) - 1),
	};
	static const struct bt_data sd[] = {
		BT_DATA(BT_DATA_UUID128_ALL, (uint8_t *)&nus_uuid.val,
			sizeof(nus_uuid.val)),
	};
	int err;

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed: %d", err);
		return err;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Bluetooth advertising failed: %d", err);
		return err;
	}

	LOG_INF("BLE NUS shell started");
	return 0;
}
