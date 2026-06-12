#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include "host/ble_hs.h"

// int ble_manager_gap_event(struct ble_gap_event *event, void *arg);

void ble_manager_init(void);

void ble_manager_start(void);

void ble_manager_stop(void);

int get_ble_status(void);

#endif