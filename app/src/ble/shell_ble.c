#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/util.h>

#include "ble/ble.h"

LOG_MODULE_REGISTER(shell_ble, LOG_LEVEL_INF);

#define SHELL_BLE_PROMPT "c406pro> "
#define SHELL_BLE_RX_BUF_SIZE 256
#define SHELL_BLE_LOG_QUEUE_SIZE 512
#define SHELL_BLE_LOG_QUEUE_TIMEOUT 100

struct shell_ble_transport
{
    shell_transport_handler_t handler;
    void *context;
    struct ring_buf rx_ringbuf;
    uint8_t rx_buf[SHELL_BLE_RX_BUF_SIZE];
    bool initialized;
};

static struct shell_ble_transport transport_ctx;
static bool notify_enabled;
static bool subscribed_once;
static const struct shell shell_ble;

static void tx_ready(void)
{
    if (transport_ctx.handler)
    {
        transport_ctx.handler(SHELL_TRANSPORT_EVT_TX_RDY, transport_ctx.context);
    }
}

static void tx_retry(struct k_work *work)
{
    ARG_UNUSED(work);

    if (notify_enabled)
    {
        tx_ready();
    }
}

K_WORK_DELAYABLE_DEFINE(tx_retry_work, tx_retry);

static void nus_received(struct bt_conn *conn, const void *data, uint16_t len, void *context)
{
    uint32_t written;

    ARG_UNUSED(conn);
    ARG_UNUSED(context);

    written = ring_buf_put(&transport_ctx.rx_ringbuf, data, len);
    if (written != len)
    {
        LOG_WRN("Shell RX ring buffer full");
    }

    if (written && transport_ctx.handler)
    {
        transport_ctx.handler(SHELL_TRANSPORT_EVT_RX_RDY, transport_ctx.context);
    }
}

static void nus_notif_enabled(bool enabled, void *context)
{
    ARG_UNUSED(context);

    notify_enabled = enabled;
    if (enabled)
    {
        tx_ready();
        if (subscribed_once)
        {
            shell_fprintf(&shell_ble, SHELL_NORMAL, "");
        }
        subscribed_once = true;
    }
}

static struct bt_nus_cb nus_listener = {
    .notif_enabled = nus_notif_enabled,
    .received = nus_received,
};

static int init(const struct shell_transport *transport, const void *config, shell_transport_handler_t handler,
                void *context)
{
    struct shell_ble_transport *ctx = transport->ctx;

    ARG_UNUSED(config);

    if (ctx->initialized)
    {
        return -EINVAL;
    }

    ctx->handler = handler;
    ctx->context = context;
    ring_buf_init(&ctx->rx_ringbuf, sizeof(ctx->rx_buf), ctx->rx_buf);
    ctx->initialized = true;
    return 0;
}

static int uninit(const struct shell_transport *transport)
{
    struct shell_ble_transport *ctx = transport->ctx;

    if (!ctx->initialized)
    {
        return -ENODEV;
    }

    ctx->initialized = false;
    return 0;
}

static int enable(const struct shell_transport *transport, bool blocking_tx)
{
    struct shell_ble_transport *ctx = transport->ctx;

    ARG_UNUSED(blocking_tx);

    return ctx->initialized ? 0 : -ENODEV;
}

static int write(const struct shell_transport *transport, const void *data, size_t len, size_t *written)
{
    struct shell_ble_transport *ctx = transport->ctx;
    struct bt_conn *conn = ble_conn();
    size_t chunk;
    int err;

    *written = 0;
    if (!ctx->initialized)
    {
        return -ENODEV;
    }

    if (!conn || !notify_enabled)
    {
        return 0;
    }

    chunk = MIN(len, MAX(1, bt_gatt_get_mtu(conn) - 3));
    err = bt_nus_send(conn, data, chunk);
    if (!err)
    {
        *written = chunk;
        return 0;
    }

    if (err == -ENOMEM)
    {
        k_work_reschedule(&tx_retry_work, K_MSEC(2));
        return 0;
    }

    return (err == -ENOTCONN || err == -EAGAIN) ? 0 : err;
}

static int read(const struct shell_transport *transport, void *data, size_t len, size_t *count)
{
    struct shell_ble_transport *ctx = transport->ctx;

    if (!ctx->initialized)
    {
        *count = 0;
        return -ENODEV;
    }

    *count = ring_buf_get(&ctx->rx_ringbuf, data, len);
    return 0;
}

static const struct shell_transport_api transport_api = {
    .init = init,
    .uninit = uninit,
    .enable = enable,
    .write = write,
    .read = read,
};

static const struct shell_transport transport = {
    .api = &transport_api,
    .ctx = &transport_ctx,
};

SHELL_DEFINE(shell_ble, SHELL_BLE_PROMPT, &transport, SHELL_BLE_LOG_QUEUE_SIZE, SHELL_BLE_LOG_QUEUE_TIMEOUT,
             SHELL_FLAG_OLF_CRLF);

static int shell_ble_init(void)
{
    static const struct shell_backend_config_flags flags = SHELL_DEFAULT_BACKEND_CONFIG_FLAGS;
    int err;

    err = shell_init(&shell_ble, NULL, flags, false, 0);
    if (err)
    {
        return err;
    }

    return bt_nus_cb_register(&nus_listener, NULL);
}

SYS_INIT(shell_ble_init, POST_KERNEL, 0);
