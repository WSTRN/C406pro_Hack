#ifndef BLE_NUS_H_
#define BLE_NUS_H_

#include <stddef.h>
#include <stdint.h>

#include <zephyr/bluetooth/uuid.h>

#define BLE_NUS_SERVICE_UUID_BYTES \
	BT_UUID_128_ENCODE(0x6e400001, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)
#define BLE_NUS_RX_UUID_BYTES \
	BT_UUID_128_ENCODE(0x6e400002, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)
#define BLE_NUS_TX_UUID_BYTES \
	BT_UUID_128_ENCODE(0x6e400003, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)

typedef void (*ble_nus_rx_handler_t)(const uint8_t *data, size_t len,
				     void *user_data);

void ble_nus_set_rx_handler(ble_nus_rx_handler_t handler, void *user_data);
int ble_nus_send(const uint8_t *data, size_t len, size_t *sent);

#endif /* BLE_NUS_H_ */
