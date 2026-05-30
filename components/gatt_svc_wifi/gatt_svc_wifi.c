#include "gatt_svc_wifi.h"
#include "nvs_manager.h"

#include <string.h>
#include "esp_log.h"

static char wifi_ssid[32 + 1] = {0};
static char wifi_pass[64 + 1] = {0};

static const char *TAG = "GATT_SVC_WIFI";

// Callback of SSID's wifi throught BLE
int ble_wifi_ssid_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR)
        return BLE_ATT_ERR_UNLIKELY;

    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    ble_hs_mbuf_to_flat(ctxt->om, wifi_ssid, sizeof(wifi_ssid) - 1, NULL);
    wifi_ssid[len] = '\0';

    ESP_LOGI(TAG, "SSID received: \"%s\"", wifi_ssid);
    nvs_manager_save_ssid(wifi_ssid);
    return 0;
}

// Callback of password's wifi throught BLE
int ble_wifi_pw_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR)
        return BLE_ATT_ERR_UNLIKELY;

    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    ble_hs_mbuf_to_flat(ctxt->om, wifi_pass, sizeof(wifi_pass) - 1, NULL);
    wifi_pass[len] = '\0';

    ESP_LOGI(TAG, "Password received: \"%s\"", wifi_pass);
    nvs_manager_save_password(wifi_pass);
    return 0;
}