#ifndef PROV_PROFILE_H
#define PROV_PROFILE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef enum
{
    PROV_PROFILE_EVT_RX_SSID,
    PROV_PROFILE_EVT_RX_PASSWORD,
    PROV_PROFILE_EVT_CMD_CONNECT,
    PROV_PROFILE_EVT_CMD_DISCONNECT,
} prov_profile_evt_t;

typedef struct
{
    prov_profile_evt_t event;
    uint16_t conn_handle;
    uint8_t *data;
    uint16_t len;
} prov_profile_cb_param_t;

typedef int (*prov_profile_cb_t)(prov_profile_cb_param_t *param);

esp_err_t prov_profile_init(void);

esp_err_t prov_profile_register_cb(prov_profile_cb_t cb);

#endif