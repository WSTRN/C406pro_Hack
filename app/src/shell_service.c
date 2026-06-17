#include "shell_service.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/util.h>

#include "ble/ble_nus.h"

LOG_MODULE_REGISTER(shell_service, LOG_LEVEL_INF);

#define SHELL_SERVICE_PROMPT "c406pro> "
#define SHELL_SERVICE_RX_BUF_SIZE 256
#define SHELL_SERVICE_LOG_QUEUE_SIZE 512
#define SHELL_SERVICE_LOG_QUEUE_TIMEOUT 100

struct shell_service_transport {
	shell_transport_handler_t handler;
	void *context;
	struct ring_buf rx_ringbuf;
	uint8_t rx_buf[SHELL_SERVICE_RX_BUF_SIZE];
	bool initialized;
	bool blocking_tx;
};

static struct shell_service_transport transport_ctx;

static void shell_rx_received(const uint8_t *data, size_t len, void *user_data)
{
	uint32_t written;

	ARG_UNUSED(user_data);

	written = ring_buf_put(&transport_ctx.rx_ringbuf, data, len);
	if (written != len) {
		LOG_WRN("Shell RX ring buffer full");
	}

	if (written && transport_ctx.handler) {
		transport_ctx.handler(SHELL_TRANSPORT_EVT_RX_RDY,
				      transport_ctx.context);
	}
}

static int transport_init(const struct shell_transport *transport,
			  const void *config,
			  shell_transport_handler_t evt_handler,
			  void *context)
{
	struct shell_service_transport *service = transport->ctx;

	ARG_UNUSED(config);

	if (service->initialized) {
		return -EINVAL;
	}

	service->handler = evt_handler;
	service->context = context;
	ring_buf_init(&service->rx_ringbuf, sizeof(service->rx_buf),
		      service->rx_buf);
	service->initialized = true;

	return 0;
}

static int transport_uninit(const struct shell_transport *transport)
{
	struct shell_service_transport *service = transport->ctx;

	if (!service->initialized) {
		return -ENODEV;
	}

	service->initialized = false;
	return 0;
}

static int transport_enable(const struct shell_transport *transport,
			    bool blocking_tx)
{
	struct shell_service_transport *service = transport->ctx;

	if (!service->initialized) {
		return -ENODEV;
	}

	service->blocking_tx = blocking_tx;
	return 0;
}

static int transport_write(const struct shell_transport *transport,
			   const void *data, size_t len, size_t *cnt)
{
	struct shell_service_transport *service = transport->ctx;
	size_t sent = 0;
	int err;

	if (!service->initialized) {
		*cnt = 0;
		return -ENODEV;
	}

	err = ble_nus_send(data, len, &sent);
	*cnt = sent;

	if (service->handler) {
		service->handler(SHELL_TRANSPORT_EVT_TX_RDY, service->context);
	}

	return err;
}

static int transport_read(const struct shell_transport *transport,
			  void *data, size_t len, size_t *cnt)
{
	struct shell_service_transport *service = transport->ctx;

	if (!service->initialized) {
		*cnt = 0;
		return -ENODEV;
	}

	*cnt = ring_buf_get(&service->rx_ringbuf, data, len);
	return 0;
}

static const struct shell_transport_api shell_service_transport_api = {
	.init = transport_init,
	.uninit = transport_uninit,
	.enable = transport_enable,
	.write = transport_write,
	.read = transport_read,
};

static const struct shell_transport shell_service_transport = {
	.api = &shell_service_transport_api,
	.ctx = &transport_ctx,
};

SHELL_DEFINE(shell_service, SHELL_SERVICE_PROMPT, &shell_service_transport,
	     SHELL_SERVICE_LOG_QUEUE_SIZE,
	     SHELL_SERVICE_LOG_QUEUE_TIMEOUT,
	     SHELL_FLAG_OLF_CRLF);

static int shell_service_init(void)
{
	struct shell_backend_config_flags cfg_flags =
		SHELL_DEFAULT_BACKEND_CONFIG_FLAGS;

	shell_init(&shell_service, NULL, cfg_flags, false, 0);
	return 0;
}

SYS_INIT(shell_service_init, POST_KERNEL, 0);

int shell_service_start(void)
{
	ble_nus_set_rx_handler(shell_rx_received, NULL);
	return 0;
}
