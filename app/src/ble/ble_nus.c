#include "ble/ble_nus.h"

#include <stddef.h>
#include <stdint.h>

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "ble/ble.h"

LOG_MODULE_REGISTER(ble_nus, LOG_LEVEL_INF);

static struct bt_uuid_128 nus_uuid =
	BT_UUID_INIT_128(BLE_NUS_SERVICE_UUID_BYTES);
static struct bt_uuid_128 nus_rx_uuid =
	BT_UUID_INIT_128(BLE_NUS_RX_UUID_BYTES);
static struct bt_uuid_128 nus_tx_uuid =
	BT_UUID_INIT_128(BLE_NUS_TX_UUID_BYTES);

static ble_nus_rx_handler_t rx_handler;
static void *rx_user_data;

K_MUTEX_DEFINE(nus_tx_lock);

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

void ble_nus_set_rx_handler(ble_nus_rx_handler_t handler, void *user_data)
{
	rx_handler = handler;
	rx_user_data = user_data;
}

static void nus_tx_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	ARG_UNUSED(value);
}

static ssize_t nus_rx_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			    const void *buf, uint16_t len, uint16_t offset,
			    uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(flags);

	if (offset != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	if (rx_handler) {
		rx_handler(buf, len, rx_user_data);
	}

	return len;
}

int ble_nus_send(const uint8_t *data, size_t len, size_t *sent)
{
	struct bt_conn *conn = ble_conn();
	int err;

	if (sent) {
		*sent = 0;
	}

	if (!conn || !bt_gatt_is_subscribed(conn, &nus_svc.attrs[4],
					    BT_GATT_CCC_NOTIFY)) {
		if (sent) {
			*sent = len;
		}
		return 0;
	}

	k_mutex_lock(&nus_tx_lock, K_FOREVER);
	err = bt_gatt_notify(conn, &nus_svc.attrs[4], data, len);
	k_mutex_unlock(&nus_tx_lock);

	if (!err && sent) {
		*sent = len;
	}

	return err;
}
