#ifndef STORAGE_SERVICE_H
#define STORAGE_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

esp_err_t storage_service_init(void);

esp_err_t storage_service_save_wifi(const char *ssid, const char *password);

bool storage_service_read_wifi(char *ssid, size_t max_ssid_len, char *password, size_t max_pw_len);

esp_err_t storage_service_save_ssid(const char *ssid);

esp_err_t storage_service_save_password(const char *password);

#endif