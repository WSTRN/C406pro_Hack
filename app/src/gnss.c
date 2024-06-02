#include "gnss.h"

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <string.h>

#include "zephyr/kernel/thread.h"

LOG_MODULE_REGISTER(gnss, LOG_LEVEL_INF);

#define GNSS_STACK_SIZE 2048
#define GNSS_PRIORITY 5
K_THREAD_STACK_DEFINE(gnss_stack_area, GNSS_STACK_SIZE);
struct k_thread gnss_thread_data;
k_tid_t gnss_tid;

static const struct device *const uart_dev = DEVICE_DT_GET(DT_ALIAS(gnss_uart));

static char rx_buf[128];
static int rx_buf_pos;
K_MSGQ_DEFINE(uart_msgq, 128, 3, 4);

void print_uart(char *buf)
{
    int msg_len = strlen(buf);

    for (int i = 0; i < msg_len; i++)
    {
        uart_poll_out(uart_dev, buf[i]);
    }
}

void serial_cb(const struct device *dev, void *user_data)
{
    uint8_t c;

    if (!uart_irq_update(uart_dev))
    {
        return;
    }

    if (!uart_irq_rx_ready(uart_dev))
    {
        return;
    }

    /* read until FIFO empty */
    while (uart_fifo_read(uart_dev, &c, 1) == 1)
    {
        if ((c == '\n' || c == '\r') && rx_buf_pos > 0)
        {
            /* terminate string */
            rx_buf[rx_buf_pos] = '\0';

            /* if queue is full, message is silently dropped */
            k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);

            /* reset the buffer (it was copied to the msgq) */
            rx_buf_pos = 0;
        }
        else if (rx_buf_pos < (sizeof(rx_buf) - 1))
        {
            rx_buf[rx_buf_pos++] = c;
        }
        /* else: characters beyond buffer size are dropped */
    }
}

void gnss_entry_point(void *, void *, void *)
{
    // gnss code here
    char tx_buf[] = "$PDTINFO\r\n";
	char gnss_buf[128];

    if (!device_is_ready(uart_dev))
    {
        printk("UART device not found!");
        return;
    }

    /* configure interrupt and callback to receive data */
    int ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);

    if (ret < 0)
    {
        if (ret == -ENOTSUP)
        {
            printk("Interrupt-driven UART API support not enabled\n");
        }
        else if (ret == -ENOSYS)
        {
            printk("UART device does not support interrupt-driven API\n");
        }
        else
        {
            printk("Error setting UART callback: %d\n", ret);
        }
        return;
    }
    uart_irq_rx_enable(uart_dev);

    for (;;)
    {
        print_uart(tx_buf);
        while (k_msgq_get(&uart_msgq, &gnss_buf, K_FOREVER) == 0)
        {
			LOG_INF("%s\r\n", gnss_buf);
        }
		k_sleep(K_MSEC(1000));
    }
}

void gnss_init()
{
    // gnss init code here
    gnss_tid = k_thread_create(&gnss_thread_data, gnss_stack_area, K_THREAD_STACK_SIZEOF(gnss_stack_area),
                               gnss_entry_point, NULL, NULL, NULL, GNSS_PRIORITY, 0, K_NO_WAIT);
}
