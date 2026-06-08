#include "ble_gatts_svc.h"
#include <stdio.h>
#include "esp_log.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "gatt_svc_wifi.h"

static const char *TAG = "GATT_SVC";

//  76c3e5d6-ee9e-4f48-bb4d-287d88f872d6
static const ble_uuid128_t wifi_info_uuid = BLE_UUID128_INIT(0xd6, 0x72, 0xf8, 0x88, 0x7d, 0x28, 0x4d, 0xbb,
                                                             0x48, 0x4f, 0x9e, 0xee, 0xd6, 0xe5, 0xc3, 0x76);
//  6fc254b7-6bc0-475b-b836-720d2287922d
static const ble_uuid128_t wifi_ssid_info_uuid = BLE_UUID128_INIT(0x2d, 0x92, 0x87, 0x22, 0x0d, 0x72, 0x36, 0xb8,
                                                                  0x5b, 0x47, 0xc0, 0x6b, 0xb7, 0x54, 0xc2, 0x6f);
//  cfedbbc2-0598-4581-b1fd-b7d54fec0396
static const ble_uuid128_t wifi_pw_uuid = BLE_UUID128_INIT(0x96, 0x03, 0xec, 0x4f, 0xd5, 0xb7, 0xfd, 0xb1,
                                                           0x81, 0x45, 0x98, 0x05, 0xc2, 0xbb, 0xed, 0xcf);
static uint8_t gatt_svr_sec_test_static_val;

// GATT table
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        // Service: wifi info
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &wifi_info_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                // Characteristic: save ssid
                .uuid = &wifi_ssid_info_uuid.u,
                .access_cb = ble_wifi_ssid_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
            },
            {
                // Characteristic: save password
                .uuid = &wifi_pw_uuid.u,
                .access_cb = ble_wifi_pw_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
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

int gatt_svc_init(void)
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "ble_gatts_count_cfg rc=%d", rc);
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "ble_gatts_add_svcs rc=%d", rc);
        return rc;
    }

    return 0;
}
