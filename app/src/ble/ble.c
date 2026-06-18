#include "ble/ble.h"

#include <errno.h>

#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(ble, LOG_LEVEL_INF);

static struct bt_conn *active_conn;
static atomic_t advertising_enabled = ATOMIC_INIT(1);

static const uint8_t ad_flags[] = {
    BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR,
};
static const uint8_t service_uuids[] = {
    BT_UUID_NUS_SRV_VAL,
};
static const struct bt_data ad[] = {
    BT_DATA(BT_DATA_FLAGS, ad_flags, sizeof(ad_flags)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};
static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_UUID128_ALL, service_uuids, sizeof(service_uuids)),
};

static int advertising_start(void)
{
    int err;

    if (!atomic_get(&advertising_enabled) || active_conn)
    {
        return 0;
    }

    err = bt_le_adv_start(BT_LE_ADV_CONN_ONE_TIME, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    return err == -EALREADY ? 0 : err;
}

static void advertising_start_work_handler(struct k_work *work)
{
    int err;

    ARG_UNUSED(work);

    err = advertising_start();
    if (err)
    {
        LOG_ERR("Bluetooth advertising restart failed: %d", err);
    }
}

K_WORK_DEFINE(advertising_start_work, advertising_start_work_handler);

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err)
    {
        LOG_WRN("Bluetooth connection failed: %u", err);
        return;
    }

    if (!active_conn)
    {
        active_conn = bt_conn_ref(conn);
    }

    LOG_INF("Bluetooth connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    ARG_UNUSED(conn);

    if (active_conn)
    {
        bt_conn_unref(active_conn);
        active_conn = NULL;
    }

    LOG_INF("Bluetooth disconnected: %u", reason);
}

static void recycled(void)
{
    k_work_submit(&advertising_start_work);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .recycled = recycled,
};

struct bt_conn *ble_conn(void)
{
    return active_conn;
}

bool ble_advertising_is_enabled(void)
{
    return atomic_get(&advertising_enabled);
}

int ble_advertising_set_enabled(bool enabled)
{
    atomic_set(&advertising_enabled, enabled);
    return enabled ? advertising_start() : bt_le_adv_stop();
}

int ble_start(void)
{
    int err;

    err = bt_enable(NULL);
    if (err)
    {
        LOG_ERR("Bluetooth init failed: %d", err);
        return err;
    }

    err = advertising_start();
    if (err)
    {
        LOG_ERR("Bluetooth advertising failed: %d", err);
        return err;
    }

    LOG_INF("Bluetooth started");
    return 0;
}
