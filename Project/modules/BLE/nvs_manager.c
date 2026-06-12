#include "nvs_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "NVS_MANAGER";
char ssid[33] = {0};
char password[65] = {0};

void nvs_manager_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        err = nvs_flash_init();
    }

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "NVS flash initialized");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize NVS flash");
    }

    // check stored wifi creds
    if (nvs_manager_read_wifi(ssid, sizeof(ssid), password, sizeof(password)))
    {
        ESP_LOGI(TAG, "Found stored wifi SSID: '%s', PW: '%s'", ssid, password);
    }
    else
    {
        ESP_LOGI(TAG, "No wifi credentials found in flash");
    }
}

void nvs_manager_save_wifi(const char *ssid, const char *password)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi_store", NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle for writing");
        return;
    }

    nvs_set_str(handle, "ssid", ssid);
    nvs_set_str(handle, "password", password);
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "Successfully saved credentials to NVS");
}

bool nvs_manager_read_wifi(char *ssid, size_t max_ssid_len, char *password, size_t max_pw_len)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi_store", NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        return false;
    }

    size_t ssid_len = max_ssid_len;
    err = nvs_get_str(handle, "ssid", ssid, &ssid_len);
    if (err != ESP_OK)
    {
        nvs_close(handle);
        return false;
    }

    size_t pw_len = max_pw_len;
    err = nvs_get_str(handle, "password", password, &pw_len);
    if (err != ESP_OK)
    {
        nvs_close(handle);
        return false;
    }

    nvs_close(handle);
    return true;
}

void nvs_manager_save_ssid(const char *ssid)
{
    nvs_handle_t handle;
    nvs_open("wifi_store", NVS_READWRITE, &handle);
    nvs_set_str(handle, "ssid", ssid);
    nvs_commit(handle);
    nvs_close(handle);
}

void nvs_manager_save_password(const char *password)
{
    nvs_handle_t handle;
    nvs_open("wifi_store", NVS_READWRITE, &handle);
    nvs_set_str(handle, "password", password);
    nvs_commit(handle);
    nvs_close(handle);
}