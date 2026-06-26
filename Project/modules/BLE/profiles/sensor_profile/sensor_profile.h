#ifndef SENSOR_PROFILE_H
#define SENSOR_PROFILE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef enum
{
    SENSOR_PROFILE_EVT_READ_DATA,
    SENSOR_PROFILE_EVT_READ_CONFIG,
    SENSOR_PROFILE_EVT_WRITE_CONFIG,
} sensor_profile_evt_t;

typedef struct
{
    sensor_profile_evt_t event;
    uint16_t conn_handle;
    uint8_t *data;
    uint16_t len;
    uint16_t max_len;
} sensor_profile_cb_param_t;

typedef int (*sensor_profile_cb_t)(sensor_profile_cb_param_t *param);

esp_err_t sensor_profile_init(void);

esp_err_t sensor_profile_register_cb(sensor_profile_cb_t cb);

esp_err_t sensor_profile_notify_data(uint16_t conn_handle, const uint8_t *data, uint16_t len);

#endif
