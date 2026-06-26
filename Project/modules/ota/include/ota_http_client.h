#ifndef OTA_HTTP_CLIENT_H
#define OTA_HTTP_CLIENT_H

#include "esp_err.h"
#include <stdbool.h>

typedef struct
{
    const char *url;
    const char *cert_pem;
    bool skip_cert_verify;
} ota_http_cfg_t;

esp_err_t ota_http_client_start(const ota_http_cfg_t *cfg);

#endif