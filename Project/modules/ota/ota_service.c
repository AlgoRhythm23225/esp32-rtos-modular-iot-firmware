#include "ota_service.h"
#include "ota_http_client.h"
#include "ota_agent.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <string.h>
#include "rtos_config.h"

static const char *TAG = "OTA_SERVICE";

static SemaphoreHandle_t s_ota_mutex = NULL;
static bool s_ota_busy = false;

static ota_agent_ctx_t s_passive_ota_ctx;

typedef struct
{
    char url[256];
    char version[32];
} ota_task_params_t;

esp_err_t ota_service_init(void)
{
    if (s_ota_mutex == NULL)
    {
        s_ota_mutex = xSemaphoreCreateMutex();
        if (s_ota_mutex == NULL)
        {
            ESP_LOGE(TAG, "Failed to create OTA mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    s_ota_busy = false;
    ESP_LOGI(TAG, "OTA service initialized");
    return ESP_OK;
}

bool ota_service_is_busy(void)
{
    bool busy = false;
    if (s_ota_mutex != NULL && xSemaphoreTake(s_ota_mutex, portMAX_DELAY) == pdTRUE)
    {
        busy = s_ota_busy;
        xSemaphoreGive(s_ota_mutex);
    }
    return busy;
}

static void ota_http_task(void *arg)
{
    ota_task_params_t *params = (ota_task_params_t *)arg;

    ESP_LOGI(TAG, "Starting HTTP OTA task for version %s", params->version);

    ota_http_cfg_t http_cfg = {
        .url = params->url,
        .cert_pem = NULL,
        .skip_cert_verify = true};

    esp_err_t err = ota_http_client_start(&http_cfg);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP OTA successful! Rebooting in 2s...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }
    else
    {
        ESP_LOGE(TAG, "HTTP OTA failed: %s", esp_err_to_name(err));
    }

    free(params);
    vTaskDelete(NULL);
}

esp_err_t ota_service_trigger_http(const char *url, const char *version)
{
    if (ota_service_is_busy())
    {
        ESP_LOGW(TAG, "OTA is already running");
        return ESP_ERR_INVALID_STATE;
    }

    if (url == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ota_task_params_t *params = malloc(sizeof(ota_task_params_t));
    if (params == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    strncpy(params->url, url, sizeof(params->url) - 1);
    params->url[sizeof(params->url) - 1] = '\0';

    if (version)
    {
        strncpy(params->version, version, sizeof(params->version) - 1);
        params->version[sizeof(params->version) - 1] = '\0';
    }
    else
    {
        params->version[0] = '\0';
    }

    if (xTaskCreate(ota_http_task, "ota_http_task", 8192, params, 5, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create HTTP OTA task");
        free(params);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ota_service_begin(uint32_t firmware_size)
{
    if (s_ota_mutex == NULL || xSemaphoreTake(s_ota_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ota_busy)
    {
        ESP_LOGW(TAG, "OTA is already running");
        xSemaphoreGive(s_ota_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    s_ota_busy = true;
    ESP_LOGI(TAG, "Starting OTA... expected size: %lu", firmware_size);

    memset(&s_passive_ota_ctx, 0, sizeof(s_passive_ota_ctx));
    s_passive_ota_ctx.cfg.firmware_size = firmware_size;
    s_passive_ota_ctx.cfg.skip_sha256 = true; // Can be enhanced to receive hash from client

    esp_err_t err = ota_agent_init(&s_passive_ota_ctx);
    if (err != ESP_OK)
    {
        s_ota_busy = false;
        xSemaphoreGive(s_ota_mutex);
        return err;
    }

    err = ota_agent_start(&s_passive_ota_ctx);
    if (err != ESP_OK)
    {
        ota_agent_deinit(&s_passive_ota_ctx);
        s_ota_busy = false;
        xSemaphoreGive(s_ota_mutex);
        return err;
    }

    xSemaphoreGive(s_ota_mutex);
    return ESP_OK;
}

esp_err_t ota_service_write(const uint8_t *data, size_t length)
{
    if (s_ota_mutex == NULL || xSemaphoreTake(s_ota_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_ota_busy || s_passive_ota_ctx.state != OTA_STATE_RUNNING)
    {
        xSemaphoreGive(s_ota_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ota_agent_write(&s_passive_ota_ctx, data, length);

    xSemaphoreGive(s_ota_mutex);
    return err;
}

esp_err_t ota_service_finish(void)
{
    if (s_ota_mutex == NULL || xSemaphoreTake(s_ota_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_ota_busy || s_passive_ota_ctx.state != OTA_STATE_RUNNING)
    {
        xSemaphoreGive(s_ota_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ota_agent_finish(&s_passive_ota_ctx);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "OTA completed successfully! Device ready for manual reboot.");
    }
    else
    {
        ESP_LOGE(TAG, "OTA finish failed: %s", esp_err_to_name(err));
    }

    ota_agent_deinit(&s_passive_ota_ctx);
    s_ota_busy = false;

    xSemaphoreGive(s_ota_mutex);
    return err;
}

esp_err_t ota_service_abort(void)
{
    if (s_ota_mutex == NULL || xSemaphoreTake(s_ota_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_ota_busy)
    {
        xSemaphoreGive(s_ota_mutex);
        return ESP_OK;
    }

    esp_err_t err = ota_agent_abort(&s_passive_ota_ctx);
    ota_agent_deinit(&s_passive_ota_ctx);
    s_ota_busy = false;
    xSemaphoreGive(s_ota_mutex);

    ESP_LOGW(TAG, "OTA aborted.");
    return err;
}

void ota_service_trigger(ota_trigger_method_t method, const ota_trigger_params_t *params)
{
    if (params == NULL)
    {
        ESP_LOGE(TAG, "OTA trigger failed: params is NULL");
        return;
    }

    switch (method)
    {
    case OTA_VIA_HTTP:
        ESP_LOGI(TAG, "Triggering OTA via HTTP (URL: %s, Version: %s)",
                 params->url ? params->url : "NULL",
                 params->version ? params->version : "UNKNOWN");

        EventBits_t bits = xEventGroupGetBits(g_system_event_group);
        if ((bits & EVT_WIFI_CONNECTED) != 0)
        {
            esp_err_t err = ota_service_trigger_http(params->url, params->version);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to start HTTP OTA task: %s", esp_err_to_name(err));
            }
        }
        else
        {
            ESP_LOGW(TAG, "Condition failed: Cannot trigger HTTP OTA. Wifi is offline!");
        }
        break;

    case OTA_VIA_BLE_OFFLINE:
        if (params->ble_event_type == 0)
        {
            ESP_LOGI(TAG, "Trigger OTA via BLE offline. Expected size: %lu", params->fw_size);
            ota_service_begin(params->fw_size);
        }
        else if (params->ble_event_type == 1)
        {
            ota_service_write(params->data, params->len);
        }
        else if (params->ble_event_type == 2)
        {
            ESP_LOGI(TAG, "Trigger OTA via BLE offline: Finish");
            ota_service_finish();
        }
        break;

    default:
        ESP_LOGE(TAG, "Unknown OTA trigger method!");
        break;
    }
}
