#ifndef BLE_H_
#define BLE_H_

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

int ble_start(void);
struct bt_conn *ble_conn(void);

#endif /* BLE_H_ */
