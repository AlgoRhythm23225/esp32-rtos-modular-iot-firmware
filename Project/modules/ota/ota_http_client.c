#include "ota_http_client.h"
#include "ota_agent.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "OTA_HTTP";

#define HTTP_OTA_CHUNK_SIZE 2048

static void ota_progress_callback(size_t bytes_written, size_t total_size, void *cb_ctx)
{
    if (total_size > 0)
    {
        ESP_LOGI(TAG, "OTA Progress: %lu / %lu bytes (%.2f%%)", (unsigned long)bytes_written, (unsigned long)total_size,
                 ((float)bytes_written / total_size) * 100.0);
    }
    else
    {
        ESP_LOGI(TAG, "OTA Progress: %lu bytes written", (unsigned long)bytes_written);
    }
}

esp_err_t ota_http_client_start(const ota_http_cfg_t *cfg)
{
    if (cfg == NULL || cfg->url == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Connecting to OTA server: %s", cfg->url);

    esp_http_client_config_t http_config = {
        .url = cfg->url,
        .cert_pem = cfg->cert_pem,
        .timeout_ms = 10000,
        .keep_alive_enable = true,
        .skip_cert_common_name_check = cfg->skip_cert_verify,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0)
    {
        ESP_LOGE(TAG, "Failed to fetch HTTP headers");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Firmware size from server: %d bytes", content_length);

    ota_agent_ctx_t ota_ctx = {0};
    ota_ctx.cfg.firmware_size = (content_length > 0) ? (uint32_t)content_length : 0;
    ota_ctx.cfg.skip_sha256 = true;
    ota_ctx.cfg.on_progress = ota_progress_callback;

    err = ota_agent_init(&ota_ctx);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize OTA agent: %s", esp_err_to_name(err));
        goto exit;
    }

    err = ota_agent_start(&ota_ctx);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start OTA agent: %s", esp_err_to_name(err));
        goto exit;
    }

    char *buffer = malloc(HTTP_OTA_CHUNK_SIZE);
    if (buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for HTTP chunk");
        err = ESP_ERR_NO_MEM;
        goto abort_ota;
    }

    int bytes_read;
    while (1)
    {
        bytes_read = esp_http_client_read(client, buffer, HTTP_OTA_CHUNK_SIZE);
        if (bytes_read < 0)
        {
            ESP_LOGE(TAG, "SSL data read error");
            err = ESP_FAIL;
            break;
        }
        else if (bytes_read == 0)
        {
            ESP_LOGI(TAG, "Connection closed, all data received");
            break;
        }

        err = ota_agent_write(&ota_ctx, (const uint8_t *)buffer, bytes_read);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "OTA agent write failed: %s", esp_err_to_name(err));
            break;
        }
    }

    free(buffer);

    if (err == ESP_OK)
    {
        err = ota_agent_finish(&ota_ctx);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "OTA agent finish failed: %s", esp_err_to_name(err));
        }
        else
        {
            ESP_LOGI(TAG, "OTA Download and validation successful!");
        }
    }

abort_ota:
    if (err != ESP_OK)
    {
        ota_agent_abort(&ota_ctx);
    }
exit:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    ota_agent_deinit(&ota_ctx);
    return err;
}