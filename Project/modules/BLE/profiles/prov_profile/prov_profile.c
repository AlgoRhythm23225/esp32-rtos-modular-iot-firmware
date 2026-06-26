#include "prov_profile.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_err.h"
#include "host/ble_hs.h"

static const char *TAG = "PROV_PROF";

// Wifi service UUID: d672f888-7d28-4dbb-484f-9eeed6e5c376
static const ble_uuid128_t wifi_info_uuid = BLE_UUID128_INIT(
    0xd6, 0x72, 0xf8, 0x88, 0x7d, 0x28, 0x4d, 0xbb,
    0x48, 0x4f, 0x9e, 0xee, 0xd6, 0xe5, 0xc3, 0x76);

// Wifi SSID characteristic UUID: 2d928722-0d72-36b8-5b47-c06bb754c26f
static const ble_uuid128_t wifi_ssid_uuid = BLE_UUID128_INIT(
    0x2d, 0x92, 0x87, 0x22, 0x0d, 0x72, 0x36, 0xb8,
    0x5b, 0x47, 0xc0, 0x6b, 0xb7, 0x54, 0xc2, 0x6f);

// Wifi password characteristic UUID: 9603ec4f-d5b7-fdb1-8145-9805c2bbedcf
static const ble_uuid128_t wifi_pw_uuid = BLE_UUID128_INIT(
    0x96, 0x03, 0xec, 0x4f, 0xd5, 0xb7, 0xfd, 0xb1,
    0x81, 0x45, 0x98, 0x05, 0xc2, 0xbb, 0xed, 0xcf);

// PROV command characteristic UUID: 9823ae51-1729-4f51-b847-112233445566
static const ble_uuid128_t prov_cmd_uuid = BLE_UUID128_INIT(
    0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x47, 0xb8,
    0x51, 0x4f, 0x29, 0x17, 0x51, 0xae, 0x23, 0x98);

static uint16_t ssid_val_handle;
static uint16_t pw_val_handle;
static uint16_t cmd_val_handle;

static prov_profile_cb_t app_cb = NULL;

static int handle_prov_ssid_write(uint16_t conn_handle, struct ble_gatt_access_ctxt *ctxt)
{
    if (app_cb)
    {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        if (om_len > 64)
        {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        uint8_t buf[65] = {0};
        int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf) - 1, &om_len);
        if (rc != 0)
        {
            return BLE_ATT_ERR_UNLIKELY;
        }

        prov_profile_cb_param_t param = {
            .event = PROV_PROFILE_EVT_RX_SSID,
            .conn_handle = conn_handle,
            .data = buf,
            .len = om_len};

        return app_cb(&param);
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static int handle_prov_pw_write(uint16_t conn_handle, struct ble_gatt_access_ctxt *ctxt)
{
    if (app_cb)
    {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        if (om_len > 64)
        {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        uint8_t buf[65] = {0};
        int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf) - 1, &om_len);
        if (rc != 0)
        {
            return BLE_ATT_ERR_UNLIKELY;
        }

        prov_profile_cb_param_t param = {
            .event = PROV_PROFILE_EVT_RX_PASSWORD,
            .conn_handle = conn_handle,
            .data = buf,
            .len = om_len};

        return app_cb(&param);
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static int handle_prov_cmd_write(uint16_t conn_handle, struct ble_gatt_access_ctxt *ctxt)
{
    if (app_cb)
    {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        if (om_len != 1)
        {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        uint8_t cmd = 0;
        int rc = ble_hs_mbuf_to_flat(ctxt->om, &cmd, 1, &om_len);
        if (rc != 0)
        {
            return BLE_ATT_ERR_UNLIKELY;
        }

        // cmd: connect wifi
        if (cmd == 0x01)
        {
            prov_profile_cb_param_t param = {
                .event = PROV_PROFILE_EVT_CMD_CONNECT,
                .conn_handle = conn_handle,
                .data = &cmd,
                .len = om_len};
            return app_cb(&param);
        }
        // cmd disconnect
        else if (cmd == 0x02)
        {
            prov_profile_cb_param_t param = {
                .event = PROV_PROFILE_EVT_CMD_DISCONNECT,
                .conn_handle = conn_handle,
                .data = &cmd,
                .len = om_len};
            return app_cb(&param);
        }
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static int prov_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR)
    {
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (attr_handle == ssid_val_handle)
    {
        return handle_prov_ssid_write(conn_handle, ctxt);
    }
    else if (attr_handle == pw_val_handle)
    {
        return handle_prov_pw_write(conn_handle, ctxt);
    }
    else if (attr_handle == cmd_val_handle)
    {
        return handle_prov_cmd_write(conn_handle, ctxt);
    }

    return BLE_ATT_ERR_UNLIKELY;
}

// GATT Service Definition
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &wifi_info_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                // WIFI SSID Characteristic
                .uuid = &wifi_ssid_uuid.u,
                .access_cb = prov_access_cb,
                .val_handle = &ssid_val_handle,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            {
                // WIFI Password Characteristic
                .uuid = &wifi_pw_uuid.u,
                .access_cb = prov_access_cb,
                .val_handle = &pw_val_handle,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            {
                // PROV Command Characteristic
                .uuid = &prov_cmd_uuid.u,
                .access_cb = prov_access_cb,
                .val_handle = &cmd_val_handle,
                .flags = BLE_GATT_CHR_F_WRITE,
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

esp_err_t prov_profile_init(void)
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

    ESP_LOGI(TAG, "Provisioning profile initialized successfully");
    return ESP_OK;
}

esp_err_t prov_profile_register_cb(prov_profile_cb_t cb)
{
    app_cb = cb;
    return ESP_OK;
}