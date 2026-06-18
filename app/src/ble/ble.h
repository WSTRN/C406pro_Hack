#ifndef BLE_H_
#define BLE_H_

#include <stdbool.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

#ifdef __cplusplus
extern "C" {
#endif

int ble_start(void);
struct bt_conn *ble_conn(void);
int ble_advertising_set_enabled(bool enabled);
bool ble_advertising_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_H_ */
