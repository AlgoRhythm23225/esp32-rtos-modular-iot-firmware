#include "ota_profile.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_err.h"
#include "host/ble_hs.h"

// Define commands for control characteristic
#define BLE_OTA_CMD_START 0x01
#define BLE_OTA_CMD_FINISH 0x02
#define BLE_OTA_CMD_ABORT 0x03

static const char *TAG = "OTA_PROF";

// OTA service UUID: d673f888-7d28-4dbb-484f-9eeed6e5c376
static const ble_uuid128_t ota_svc_uuid = BLE_UUID128_INIT(
    0xd6, 0x73, 0xf8, 0x88, 0x7d, 0x28, 0x4d, 0xbb,
    0x48, 0x4f, 0x9e, 0xee, 0xd6, 0xe5, 0xc3, 0x76);

// OTA control characteristic UUID : 2d228722-0d72-36b8-5b47-c06bb754c26f
static const ble_uuid128_t ota_ctrl_uuid = BLE_UUID128_INIT(
    0x2d, 0x22, 0x87, 0x22, 0x0d, 0x72, 0x36, 0xb8,
    0x5b, 0x47, 0xc0, 0x6b, 0xb7, 0x54, 0xc2, 0x6f);

// OTA data characteristic UUID : 9603cc4f-d5b7-fdb1-8145-9305c2bbedcf
static const ble_uuid128_t ota_data_uuid = BLE_UUID128_INIT(
    0x96, 0x03, 0xcc, 0x4f, 0xd5, 0xb7, 0xfd, 0xb1,
    0x81, 0x45, 0x93, 0x05, 0xc2, 0xbb, 0xed, 0xcf);

static uint16_t ctrl_val_handle;
static uint16_t data_val_handle;

static ota_profile_cb_t app_cb = NULL;

static int handle_ota_ctrl_write(uint16_t conn_handle, struct ble_gatt_access_ctxt *ctxt)
{
    if (app_cb)
    {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        if (om_len == 0)
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

        uint8_t cmd = 0;
        os_mbuf_copydata(ctxt->om, 0, 1, &cmd);

        ota_profile_cb_param_t param = {
            .conn_handle = conn_handle,
            .data = &cmd,
            .len = 1};

        switch (cmd)
        {
        case BLE_OTA_CMD_START:
            param.event = OTA_PROFILE_EVT_START;
            break;
        case BLE_OTA_CMD_FINISH:
            param.event = OTA_PROFILE_EVT_FINISH;
            break;
        case BLE_OTA_CMD_ABORT:
            param.event = OTA_PROFILE_EVT_ABORT;
            break;
        default:
            ESP_LOGE(TAG, "Unknown OTA control command: 0x%02X", cmd);
            return BLE_ATT_ERR_UNLIKELY;
        }

        return app_cb(&param);
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static int handle_ota_data_write(uint16_t conn_handle, struct ble_gatt_access_ctxt *ctxt)
{
    uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
    if (om_len == 0)
    {
        return 0;
    }

    if (app_cb)
    {
        uint8_t *data_buf = malloc(om_len);
        if (data_buf == NULL)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for BLE OTA chunk");
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        os_mbuf_copydata(ctxt->om, 0, om_len, data_buf);

        ota_profile_cb_param_t param = {
            .event = OTA_PROFILE_EVT_DATA,
            .conn_handle = conn_handle,
            .data = data_buf,
            .len = om_len};

        int rc = app_cb(&param);
        free(data_buf);
        return rc;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static int ota_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR)
    {
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (attr_handle == ctrl_val_handle)
    {
        return handle_ota_ctrl_write(conn_handle, ctxt);
    }
    else if (attr_handle == data_val_handle)
    {
        return handle_ota_data_write(conn_handle, ctxt);
    }

    return BLE_ATT_ERR_UNLIKELY;
}

// GATT service def
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &ota_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                // OTA control characteristic
                .uuid = &ota_ctrl_uuid.u,
                .access_cb = ota_access_cb,
                .val_handle = &ctrl_val_handle,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            {
                // OTA data characteristic
                .uuid = &ota_data_uuid.u,
                .access_cb = ota_access_cb,
                .val_handle = &data_val_handle,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                0, // No more characteristics
            },
        },
    },
    {
        0, // No more services
    },
};

esp_err_t ota_profile_init(void)
{
    int rc;

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed, rc=%d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed, rc=%d", rc);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA profile initialized successfully");
    return ESP_OK;
}

esp_err_t ota_profile_register_cb(ota_profile_cb_t cb)
{
    app_cb = cb;
    return ESP_OK;
}