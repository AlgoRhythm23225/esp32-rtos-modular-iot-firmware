#include "ota_agent.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "mbedtls/md.h"
#include <string.h>

static const char *TAG = "OTA_AGENT";

esp_err_t ota_agent_pre_check_version(ota_agent_ctx_t *ctx)
{
    return ESP_OK;
}

// SHA256 check
static esp_err_t validate_cfg(ota_agent_cfg_t *cfg)
{
    if (cfg->skip_sha256)
    {
        ESP_LOGW(TAG, "SHA-256 verification disabled");
    }

    if (cfg->firmware_size == 0)
    {
        ESP_LOGW(TAG, "Firmware size is 0, length validation will be skipped");
    }

    return ESP_OK;
}

// create oop, check requires and running partition
esp_err_t ota_agent_init(ota_agent_ctx_t *ctx)
{
    if (ctx == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // check state
    if (ctx->state != OTA_STATE_IDLE)
    {
        ESP_LOGE(TAG, "OTA was not in IDLE state, current state :%s", ota_agent_state_str(ctx->state));
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = validate_cfg(&ctx->cfg);
    if (err != ESP_OK)
        return err;

    // version check
    if (ctx->cfg.allow_downgrade == 1 && ctx->cfg.forward_version != 0)
    {
        const esp_app_desc_t *running_version = esp_app_get_description();
        ESP_LOGI(TAG, "Running version string: %s  incoming: 0x%08lx", running_version->version, ctx->cfg.forward_version);
    }

    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (running_partition)
    {
        ESP_LOGI(TAG, "Running partition: %s  addr=0x%08lx", running_partition->label, running_partition->address);
    }

    ctx->bytes_written = 0;
    ctx->update_partition = NULL;
    ctx->update_handle = 0;

    if (ctx->sha256_ctx == NULL)
    {
        ctx->sha256_ctx = malloc(sizeof(mbedtls_md_context_t));
        if (ctx->sha256_ctx == NULL)
        {
            return ESP_ERR_NO_MEM;
        }
    }
    memset(ctx->sha256_ctx, 0, sizeof(mbedtls_md_context_t));

    ctx->state = OTA_STATE_READY;
    ESP_LOGI(TAG, "init OK fw_size=%lu", ctx->cfg.firmware_size);

    return ESP_OK;
}

// choose partition to update and begin writng
esp_err_t ota_agent_start(ota_agent_ctx_t *ctx)
{
    // check NULL
    if (ctx == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // check state
    if (ctx->state != OTA_STATE_READY)
    {
        ESP_LOGW(TAG, "OTA was not in READY state, current state :%s", ota_agent_state_str(ctx->state));
        return ESP_ERR_INVALID_STATE;
    }

    // get next update partition
    esp_err_t err;
    ctx->update_partition = esp_ota_get_next_update_partition(NULL);
    if (ctx->update_partition == NULL)
    {
        ESP_LOGE(TAG, "No OTA partition available, not found");
        ctx->state = OTA_STATE_ERROR;
        return ESP_ERR_NOT_FOUND;
    }

    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (ctx->update_partition == running_partition)
    {
        ESP_LOGE(TAG, "No OTA partition available, invalid state");
        ctx->state = OTA_STATE_ERROR;
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "Target: %s  addr=0x%08lx  size=0x%lx",
             ctx->update_partition->label,
             ctx->update_partition->address,
             ctx->update_partition->size);

    // OTA SIZE, erase
    size_t image_size = (ctx->cfg.firmware_size > 0) ? ctx->cfg.firmware_size : OTA_SIZE_UNKNOWN;
    ESP_LOGI(TAG, "Erasing partition (image_size=%u) ...", image_size);

    err = esp_ota_begin(ctx->update_partition, image_size, &ctx->update_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        ctx->update_handle = 0;
        ctx->state = OTA_STATE_ERROR;
        return err;
    }

    mbedtls_md_context_t *md_ctx = (mbedtls_md_context_t *)ctx->sha256_ctx;
    mbedtls_md_init(md_ctx);
    if (mbedtls_md_setup(md_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0) != 0 ||
        mbedtls_md_starts(md_ctx) != 0)
    {
        ESP_LOGE(TAG, "sha256_starts failed");
        mbedtls_md_free(md_ctx);
        esp_ota_abort(ctx->update_handle);
        ctx->update_handle = 0;
        ctx->state = OTA_STATE_ERROR;
        return ESP_FAIL;
    }

    ctx->bytes_written = 0;
    ctx->state = OTA_STATE_RUNNING;
    ESP_LOGI(TAG, "Flash erased successfully. Ready to receive data.");

    return ESP_OK;
}

esp_err_t ota_agent_write(ota_agent_ctx_t *ctx, const uint8_t *data, size_t length)
{
    // check in arg
    if (ctx == NULL || data == NULL || length == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // check state
    if (ctx->state != OTA_STATE_RUNNING)
    {
        ESP_LOGW(TAG, "OTA was not in RUNNING state, current state :%s", ota_agent_state_str(ctx->state));
        return ESP_ERR_INVALID_STATE;
    }

    if (ctx->cfg.firmware_size > 0 && (ctx->bytes_written + length) > (size_t)ctx->cfg.firmware_size)
    {
        ESP_LOGE(TAG, "Overflow: written=%u + chunk=%u > fw=%lu",
                 (unsigned)ctx->bytes_written,
                 (unsigned)length,
                 ctx->cfg.firmware_size);
        ctx->state = OTA_STATE_ERROR;
        return ESP_ERR_INVALID_SIZE;
    }

    // write
    esp_err_t err = esp_ota_write(ctx->update_handle, data, length);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
        ctx->state = OTA_STATE_ERROR;
        return err;
    }

    if (ctx->cfg.skip_sha256 == 0)
    {
        if (mbedtls_md_update((mbedtls_md_context_t *)ctx->sha256_ctx, data, length) != 0)
        {
            ESP_LOGE(TAG, "sha256_update failed");
            ctx->state = OTA_STATE_ERROR;
            return ESP_FAIL;
        }
    }

    ctx->bytes_written += length;
    if (ctx->cfg.on_progress)
    {
        ctx->cfg.on_progress(ctx->bytes_written,
                             ctx->cfg.firmware_size,
                             ctx->cfg.cb_ctx);
    }

    return ESP_OK;
}

esp_err_t ota_agent_finish(ota_agent_ctx_t *ctx)
{
    // check null
    if (ctx == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // check state
    if (ctx->state != OTA_STATE_RUNNING)
    {
        ESP_LOGW(TAG, " OTA was not in RUNNING state, current state :%s", ota_agent_state_str(ctx->state));
        return ESP_ERR_INVALID_STATE;
    }

    // verify size
    if (ctx->cfg.firmware_size > 0 && ctx->bytes_written != (size_t)ctx->cfg.firmware_size)
    {
        ESP_LOGE(TAG, "Size mismatch: current=%u expected=%lu", (unsigned)ctx->bytes_written, ctx->cfg.firmware_size);
        esp_ota_abort(ctx->update_handle);
        ctx->update_handle = 0;
        ctx->update_partition = NULL;
        ctx->state = OTA_STATE_ERROR;
        return ESP_ERR_INVALID_SIZE;
    }

    // verify SHA256
    if (!ctx->cfg.skip_sha256)
    {
        if (mbedtls_md_finish((mbedtls_md_context_t *)ctx->sha256_ctx, ctx->computed_sha256) != 0)
        {
            ESP_LOGE(TAG, "sha256_finish failed");
            mbedtls_md_free((mbedtls_md_context_t *)ctx->sha256_ctx);
            esp_ota_abort(ctx->update_handle);

            ctx->update_handle = 0;
            ctx->update_partition = NULL;
            ctx->state = OTA_STATE_ERROR;
            return ESP_FAIL;
        }

        static const uint8_t zero[OTA_AGENT_SHA256_LEN] = {0};
        if (memcmp(ctx->cfg.expected_sha256, zero, OTA_AGENT_SHA256_LEN) != 0)
        {
            if (memcmp(ctx->computed_sha256, ctx->cfg.expected_sha256, OTA_AGENT_SHA256_LEN) != 0)
            {
                ESP_LOGE(TAG, "SHA-256 MISMATCH — firmware corrupt or tampered");
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, ctx->computed_sha256, OTA_AGENT_SHA256_LEN, ESP_LOG_ERROR);
                mbedtls_md_free((mbedtls_md_context_t *)ctx->sha256_ctx);
                esp_ota_abort(ctx->update_handle);

                ctx->update_handle = 0;
                ctx->update_partition = NULL;
                ctx->state = OTA_STATE_ERROR;
                return ESP_ERR_INVALID_CRC;
            }
            ESP_LOGI(TAG, "SHA-256 OK");
        }
    }
    mbedtls_md_free((mbedtls_md_context_t *)ctx->sha256_ctx);

    esp_err_t err = esp_ota_end(ctx->update_handle);
    ctx->update_handle = 0;
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        ctx->update_handle = 0;
        ctx->update_partition = NULL;
        ctx->state = OTA_STATE_ERROR;
        return err;
    }

    // write otadata into bootloader
    err = esp_ota_set_boot_partition(ctx->update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "set boot partition failed: %s", esp_err_to_name(err));
        ctx->update_partition = NULL;
        ctx->state = OTA_STATE_ERROR;
        return err;
    }
    ctx->update_partition = NULL;
    ctx->state = OTA_STATE_DONE;

    ESP_LOGI(TAG, "set boot partition successed! Device is ready to restart");

    return ESP_OK;
}

esp_err_t ota_agent_abort(ota_agent_ctx_t *ctx)
{
    // check null
    if (ctx == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // check state
    if (ctx->state == OTA_STATE_IDLE || ctx->state == OTA_STATE_DONE || ctx->state == OTA_STATE_ABORTED)
    {
        return ESP_OK;
    }
    if (ctx->state == OTA_STATE_RUNNING)
    {
        mbedtls_md_free((mbedtls_md_context_t *)ctx->sha256_ctx);
    }

    // Only call esp_ota_abort if we actually have an open handle
    esp_err_t ret = ESP_OK;
    if (ctx->update_handle != 0)
    {
        // abort
        ret = esp_ota_abort(ctx->update_handle);
        ctx->update_handle = 0;

        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_abort failed: %s", esp_err_to_name(ret));
        }
    }
    ctx->update_partition = NULL;
    ctx->bytes_written = 0;
    ctx->state = OTA_STATE_ABORTED;

    ESP_LOGW(TAG, "Session aborted");
    return ret;
}

esp_err_t ota_agent_deinit(ota_agent_ctx_t *ctx)
{
    // check null
    if (ctx == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // check state
    if (ctx->state == OTA_STATE_RUNNING)
    {
        ESP_LOGE(TAG, "session still running, pls call abort() first");
        return ESP_ERR_INVALID_STATE;
    }

    if (ctx->sha256_ctx != NULL)
    {
        free(ctx->sha256_ctx);
        ctx->sha256_ctx = NULL;
    }

    memset(ctx, 0, sizeof(*ctx));
    return ESP_OK;
}

esp_err_t ota_agent_mark_valid(void)
{
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "mark_valid: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Firmware marked valid — rollback cancelled");
    }
    return err;
}

bool ota_agent_is_pending_verify(void)
{
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (!running_partition)
        return false;
    esp_ota_img_states_t s;
    if (esp_ota_get_state_partition(running_partition, &s) != ESP_OK)
        return false;
    return s == ESP_OTA_IMG_PENDING_VERIFY;
}

// roll back
bool ota_agent_can_rollback(void)
{
    return esp_ota_check_rollback_is_possible() == ESP_OK;
}

esp_err_t ota_agent_rollback(void)
{
    if (!ota_agent_can_rollback())
    {
        ESP_LOGE(TAG, "Rollback not possible");
        return ESP_FAIL;
    }
    ESP_LOGW(TAG, "Triggering rollback…");
    esp_ota_mark_app_invalid_rollback_and_reboot();
    return ESP_FAIL;
}

const char *ota_agent_state_str(ota_state_t state)
{
    switch (state)
    {
    case OTA_STATE_IDLE:
        return "IDLE";
    case OTA_STATE_READY:
        return "READY";
    case OTA_STATE_RUNNING:
        return "RUNNING";
    case OTA_STATE_DONE:
        return "DONE";
    case OTA_STATE_ERROR:
        return "ERROR";
    case OTA_STATE_ABORTED:
        return "ABORTED";
    default:
        return "UNKNOWN";
    }
}