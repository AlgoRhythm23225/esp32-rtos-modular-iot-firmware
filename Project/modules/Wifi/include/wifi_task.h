#ifndef WIFI_TASK_H
#define WIFI_TASK_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifi_start(void);
esp_err_t wifi_service_sta_connect(const char *ssid, const char *password);

#ifdef __cplusplus
}
#endif

#endif