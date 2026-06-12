#ifndef GATT_SVC_WIFI_H
#define GATT_SVC_WIFI_H

#include "host/ble_hs.h"
int ble_wifi_ssid_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg);

int ble_wifi_pw_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);
#endif