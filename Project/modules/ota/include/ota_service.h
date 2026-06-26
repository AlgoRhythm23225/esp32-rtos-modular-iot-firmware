#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum
{
    OTA_VIA_HTTP,
    OTA_VIA_BLE_OFFLINE
} ota_trigger_method_t;

typedef struct
{

    const char *url;
    const char *version;

    int ble_event_type;
    uint8_t *data;
    size_t len;
    uint32_t fw_size;
} ota_trigger_params_t;

void ota_service_trigger(ota_trigger_method_t method, const ota_trigger_params_t *params);

esp_err_t ota_service_init(void);

bool ota_service_is_busy(void);

esp_err_t ota_service_begin(uint32_t firmware_size);

esp_err_t ota_service_write(const uint8_t *data, size_t length);

esp_err_t ota_service_finish(void);

esp_err_t ota_service_abort(void);

esp_err_t ota_service_trigger_http(const char *url, const char *version);

#endif
