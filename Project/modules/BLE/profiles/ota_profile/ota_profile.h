#ifndef OTA_PROFILE_H
#define OTA_PROFILE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef enum
{
    OTA_PROFILE_EVT_START,
    OTA_PROFILE_EVT_FINISH,
    OTA_PROFILE_EVT_ABORT,
    OTA_PROFILE_EVT_DATA,
} ota_profile_evt_t;

typedef struct
{
    ota_profile_evt_t event;
    uint16_t conn_handle;
    uint8_t *data;
    uint16_t len;
} ota_profile_cb_param_t;

typedef int (*ota_profile_cb_t)(ota_profile_cb_param_t *param);

esp_err_t ota_profile_init(void);

esp_err_t ota_profile_register_cb(ota_profile_cb_t cb);

#endif