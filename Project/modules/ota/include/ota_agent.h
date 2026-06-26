#ifndef OTA_AGENT_H
#define OTA_AGENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_ota_ops.h"

#define OTA_HTTP_MAX_CHUNK_SIZE 8192u
#define OTA_BLE_MAX_CHUNK 512u
#define OTA_AGENT_SHA256_LEN 32u
typedef enum
{
    OTA_METHOD_HTTP = 0,
    OTA_METHOD_BLE,
    OTA_METHOD_UART,
    OTA_METHOD_MQTT,
} ota_method_t;

typedef void (*ota_progress_cb_t)(size_t bytes_written, size_t total_size, void *cb_ctx);

typedef enum
{
    OTA_STATE_IDLE = 0,
    OTA_STATE_READY,
    OTA_STATE_RUNNING,
    OTA_STATE_DONE,
    OTA_STATE_ERROR,
    OTA_STATE_ABORTED,
} ota_state_t;

typedef struct
{
    uint32_t firmware_size;

    // expect SHA256
    uint8_t expected_sha256[OTA_AGENT_SHA256_LEN];
    bool skip_sha256; /// true = skip

    // version
    uint32_t forward_version;
    bool allow_downgrade;

    // progress
    ota_progress_cb_t on_progress;
    void *cb_ctx;

} ota_agent_cfg_t;

typedef struct
{
    ota_agent_cfg_t cfg;

    ota_state_t state;
    size_t bytes_written;
    // partition
    const esp_partition_t *update_partition;
    esp_ota_handle_t update_handle;
    // sha256
    void *sha256_ctx;
    uint8_t computed_sha256[OTA_AGENT_SHA256_LEN];

} ota_agent_ctx_t;

esp_err_t ota_agent_init(ota_agent_ctx_t *ctx);

esp_err_t ota_agent_start(ota_agent_ctx_t *ctx);

esp_err_t ota_agent_write(ota_agent_ctx_t *ctx, const uint8_t *data, size_t length);

esp_err_t ota_agent_finish(ota_agent_ctx_t *ctx);

esp_err_t ota_agent_abort(ota_agent_ctx_t *ctx);

esp_err_t ota_agent_deinit(ota_agent_ctx_t *ctx);

esp_err_t ota_agent_mark_valid(void);

esp_err_t ota_agent_rollback(void);

bool ota_agent_is_pending_verify(void);

bool ota_agent_can_rollback(void);

const char *ota_agent_state_str(ota_state_t state);

const char *ota_agent_method_str(ota_method_t method);

#endif