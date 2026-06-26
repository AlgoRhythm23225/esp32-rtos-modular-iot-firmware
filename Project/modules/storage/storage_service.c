#include "storage_service.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "STORAGE_SERVICE";

esp_err_t storage_service_init(void)
{
    // init flash
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "NVS flash partition corrupted, erasing NVS");
        err = nvs_flash_erase();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(err));
            return err;
        }
        err = nvs_flash_init();
    }

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "NVS flash initialized");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
        return err;
    }

    // find stored credentials
    char ssid[65] = {0};
    char password[65] = {0};
    if (storage_service_read_wifi(ssid, sizeof(ssid), password, sizeof(password)))
    {
        ESP_LOGI(TAG, "Found stored Wifi SSID: %s", ssid);
        ESP_LOGI(TAG, "Found stored Wifi password: %s", password);
    }
    else
    {
        ESP_LOGI(TAG, "No Wifi credentials found in NVS flash");
    }

    return ESP_OK;
}

esp_err_t storage_service_save_wifi(const char *ssid, const char *password)
{
    // check NULL
    if (ssid == NULL || password == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi_storage", NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle wifi storage : %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(handle, "ssid", ssid);
    if (err == ESP_OK)
    {
        err = nvs_set_str(handle, "password", password);
    }

    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Successfully saved Wifi credentials to NVS");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to save Wifi credentials: %s", esp_err_to_name(err));
    }

    return err;
}

bool storage_service_read_wifi(char *ssid, size_t max_ssid_len, char *password, size_t max_pw_len)
{
    if (ssid == NULL || password == NULL || max_ssid_len == 0 || max_pw_len == 0)
    {
        return false;
    }
    // check storage
    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi_storage", NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        return false;
    }
    // check ssid
    size_t ssid_len = max_ssid_len;
    err = nvs_get_str(handle, "ssid", ssid, &ssid_len);
    if (err != ESP_OK)
    {
        nvs_close(handle);
        return false;
    }
    // check password
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

esp_err_t storage_service_save_ssid(const char *ssid)
{
    // check NULL
    if (ssid == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    // save ssid
    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi_storage", NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return err;

    err = nvs_set_str(handle, "ssid", ssid);
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

esp_err_t storage_service_save_password(const char *password)
{
    // check NULL
    if (password == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    // save ssid
    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi_storage", NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return err;

    err = nvs_set_str(handle, "password", password);
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}