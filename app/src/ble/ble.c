#include "ble/ble.h"

#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(ble, LOG_LEVEL_INF);

static struct bt_conn *active_conn;

static const uint8_t ad_flags[] = {
	BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR,
};
static const uint8_t service_uuids[] = {
	BT_UUID_NUS_SRV_VAL,
};

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
	ARG_UNUSED(conn);

	if (active_conn) {
		bt_conn_unref(active_conn);
		active_conn = NULL;
	}

	LOG_INF("Bluetooth disconnected: %u", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

struct bt_conn *ble_conn(void)
{
	return active_conn;
}

int ble_start(void)
{
	static const struct bt_data ad[] = {
		BT_DATA(BT_DATA_FLAGS, ad_flags, sizeof(ad_flags)),
		BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
			sizeof(CONFIG_BT_DEVICE_NAME) - 1),
	};
	static const struct bt_data sd[] = {
		BT_DATA(BT_DATA_UUID128_ALL, service_uuids,
			sizeof(service_uuids)),
	};
	int err;

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed: %d", err);
		return err;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Bluetooth advertising failed: %d", err);
		return err;
	}

	LOG_INF("Bluetooth started");
	return 0;
}
