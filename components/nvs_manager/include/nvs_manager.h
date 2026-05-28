#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

void nvs_manager_init(void);

void nvs_manager_save_wifi(const char *ssid, const char *password);

bool nvs_manager_read_wifi(char *ssid, size_t max_ssid_len, char *password, size_t max_pw_len);

#endif