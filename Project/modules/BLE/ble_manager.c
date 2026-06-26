#include "ble_manager.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <cJSON.h>

#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"

#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "ota_profile.h"
#include "prov_profile.h"
#include "sensor_profile.h"

#include "ota_service.h"
#include "storage_service.h"
#include "wifi_task.h"
#include "sensors_manager.h"
#include "esp_wifi.h"

#define BLE_DEVICE_NAME "ESP32-IoT"

// static int ble_service_gap_event(struct ble_gap_event *event, void *arg);
static void ble_manager_start_advertise(void);

static const char *TAG = "BLE_MANAGER";
static uint8_t own_addr_type;
static uint16_t s_active_conn_handle = 0xFFFF;

// OTA Worker Task & Queue
static QueueHandle_t s_ble_ota_queue = NULL;
static TaskHandle_t s_ble_ota_task_handle = NULL;

typedef struct
{
    ota_profile_evt_t event;
    uint8_t *data;
    uint16_t len;
} ble_ota_msg_t;

static void ble_ota_worker_task(void *param)
{
    ble_ota_msg_t msg;
    ESP_LOGI(TAG, "BLE OTA Worker Task started");
    while (1)
    {
        if (xQueueReceive(s_ble_ota_queue, &msg, portMAX_DELAY) == pdTRUE)
        {
            esp_err_t err = ESP_OK;
            switch (msg.event)
            {
            case OTA_PROFILE_EVT_START:
                ESP_LOGI(TAG, "BLE OTA Worker: START");
                err = ota_service_begin(0);
                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to start OTA: %s", esp_err_to_name(err));
                }
                break;
            case OTA_PROFILE_EVT_DATA:
                err = ota_service_write(msg.data, msg.len);
                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "OTA write failed: %s", esp_err_to_name(err));
                    ota_service_abort();
                }
                break;
            case OTA_PROFILE_EVT_FINISH:
                ESP_LOGI(TAG, "BLE OTA Worker: FINISH");
                err = ota_service_finish();
                if (err == ESP_OK)
                {
                    ESP_LOGI(TAG, "OTA progress is completely, you can reboot device");
                }
                else
                {
                    ESP_LOGE(TAG, "OTA validation failed: %s", esp_err_to_name(err));
                }
                break;
            case OTA_PROFILE_EVT_ABORT:
                ESP_LOGW(TAG, "BLE OTA Worker: ABORT");
                ota_service_abort();
                break;
            }
            if (msg.data)
            {
                free(msg.data);
            }
        }
    }
}

// profile callbacks

// PROV CALLBACK
static int prov_profile_callback(prov_profile_cb_param_t *param)
{
    char wifi_info[65] = {0};
    uint16_t len = param->len;
    if (len > 64)
    {
        len = 64;
    }
    else
    {
        len = param->len;
    }
    memcpy(wifi_info, param->data, len);
    wifi_info[len] = '\0';

    if (param->event == PROV_PROFILE_EVT_RX_SSID)
    {
        ESP_LOGI(TAG, "SSID received: %s", wifi_info);
        storage_service_save_ssid(wifi_info);
    }
    else if (param->event == PROV_PROFILE_EVT_RX_PASSWORD)
    {
        ESP_LOGI(TAG, "Password received: %s", wifi_info);
        storage_service_save_password(wifi_info);
    }
    else if (param->event == PROV_PROFILE_EVT_CMD_CONNECT)
    {
        ESP_LOGI(TAG, "GUI triggered Wi-Fi connection via Command Event");
        esp_wifi_disconnect();
        char saved_ssid[65] = {0};
        char saved_pw[65] = {0};
        if (storage_service_read_wifi(saved_ssid, sizeof(saved_ssid), saved_pw, sizeof(saved_pw)))
        {
            wifi_service_sta_connect(saved_ssid, saved_pw);
        }
    }
    else if (param->event == PROV_PROFILE_EVT_CMD_DISCONNECT)
    {
        ESP_LOGW(TAG, "GUI requested disconnection. Terminating link...");
        ble_gap_terminate(param->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }

    return 0;
}

// OTA CALLBACK
static int ota_profile_callback(ota_profile_cb_param_t *param)
{
    if (s_ble_ota_queue == NULL)
    {
        return BLE_ATT_ERR_UNLIKELY;
    }

    ble_ota_msg_t msg = {
        .event = param->event,
        .data = NULL,
        .len = param->len};

    if (param->event == OTA_PROFILE_EVT_DATA && param->len > 0)
    {
        msg.data = malloc(param->len);
        if (msg.data == NULL)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for OTA data");
            return BLE_ATT_ERR_UNLIKELY;
        }
        memcpy(msg.data, param->data, param->len);
    }

    if (xQueueSend(s_ble_ota_queue, &msg, 0) != pdTRUE)
    {
        ESP_LOGE(TAG, "OTA queue full! Dropping packet.");
        if (msg.data)
        {
            free(msg.data);
        }
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

// SENSOR CALLBNACK
static int sensor_profile_callback(sensor_profile_cb_param_t *param)
{
    if (param->event == SENSOR_PROFILE_EVT_WRITE_CONFIG)
    {
        char json_str[128] = {0};
        uint16_t len = param->len < sizeof(json_str) - 1 ? param->len : sizeof(json_str) - 1;
        memcpy(json_str, param->data, len);
        json_str[len] = '\0';

        ESP_LOGI(TAG, "Received Sensor Config: %s", json_str);

        // cJSON *root = cJSON_Parse(json_str);
        // if (root)
        // {
        //     cJSON *y = cJSON_GetObjectItem(root, "Y");
        //     cJSON *mo = cJSON_GetObjectItem(root, "Mo");
        //     cJSON *d = cJSON_GetObjectItem(root, "D");
        //     cJSON *h = cJSON_GetObjectItem(root, "H");
        //     cJSON *m = cJSON_GetObjectItem(root, "M");
        //     cJSON *s = cJSON_GetObjectItem(root, "S");

        //     if (y && mo && d && h && m && s)
        //     {
        //         sensor_manager_set_time(y->valueint, mo->valueint, d->valueint, h->valueint, m->valueint, s->valueint);
        //     }
        //     cJSON_Delete(root);
        // }
        // else
        // {
        //     ESP_LOGE(TAG, "Failed to parse Sensor Config JSON");
        // }
    }

    return 0;
}

// GAP event
static int ble_manager_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc = 0;

    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0)
        {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            ESP_LOGI(TAG, "Connection established");
            ESP_LOGI(TAG, "Peer addr: %02x:%02x:%02x:%02x:%02x:%02x",
                     desc.peer_id_addr.val[5], desc.peer_id_addr.val[4],
                     desc.peer_id_addr.val[3], desc.peer_id_addr.val[2],
                     desc.peer_id_addr.val[1], desc.peer_id_addr.val[0]);
            s_active_conn_handle = event->connect.conn_handle;
        }
        else
        {
            ESP_LOGE(TAG, "connection establishment failed, status: %d. Advertising again...", event->connect.status);
            ble_manager_start_advertise();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGW(TAG, "Device disconnected. Reason code =%d", event->disconnect.reason);
        s_active_conn_handle = 0xFFFF;
        ble_manager_start_advertise();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGD(TAG, "Advertising again...");
        ble_manager_start_advertise();
        break;

    case BLE_GAP_EVENT_MTU:
        if (event->mtu.channel_id == BLE_L2CAP_CID_ATT)
        {
            ESP_LOGI(TAG, "Conn_handle: %d | Final MTU Size: %d bytes",
                     event->mtu.conn_handle, event->mtu.value);
        }
        break;

    default:
        break;
    }

    return 0;
}

// Start advertisement
static void ble_manager_start_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    // config payload
    memset(&fields, 0, sizeof fields);
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (const uint8_t *)BLE_DEVICE_NAME;
    fields.name_len = (uint8_t)strlen(BLE_DEVICE_NAME);
    fields.name_is_complete = 1;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Error setting adv fields: %d", rc);
        return;
    }

    // start advertise
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_manager_gap_event, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Error while starting advertising: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "Advertisement started");
}

// Stop advertisement
static void ble_manager_stop_advertise(void)
{
    int rc = ble_gap_adv_stop();
    if (rc != 0 && rc != BLE_HS_EALREADY)
    {
        ESP_LOGE(TAG, "ble_gap_adv_stop failed: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "Advertisement stopped");
}

// NimBLE host task, callbacks
static void ble_hs_cfg_on_sync(void)
{
    // BT identity address set
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "device does not have any available bt address");
        return;
    }

    // BT address to use while advertising
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error determining address type; error code rc=%d", rc);
        return;
    }

    // advertise
    ble_manager_start_advertise();
}

static void ble_hs_cfg_on_reset(int reason)
{
    ESP_LOGI(TAG, "BLE Reset, reason: %d", reason);
}

static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

//
static void ble_notify_task(void *param)
{
    ESP_LOGI(TAG, "BLE Notify task started");
    while (1)
    {
        if (s_active_conn_handle != 0xFFFF)
        {
            struct ble_gap_conn_desc desc;
            if (ble_gap_conn_find(s_active_conn_handle, &desc) != 0)
            {
                s_active_conn_handle = 0xFFFF;
                continue;
            }

            // Random value for testing
            float temp = 27.8f;

            uint16_t year = 2026;
            uint8_t month = 6;
            uint8_t date = 24;
            uint8_t hour = 13;
            uint8_t minute = 42;
            uint8_t second = 17;    
            // sensor_manager_get_raw_data(&temp, &year, &month, &date, &hour, &minute, &second);

            const char* is_wifi = "OK";
            const char* is_net = "OK";

            char payload[128];
            int len = snprintf(payload, sizeof(payload),
                               "{\"Wifi\":%s,\"Net\":%s,\"T\":%.2f,\"Time\":\"%04d-%02d-%02d %02d:%02d:%02d\"}",
                               is_wifi, is_net, temp, year, month, date, hour, minute, second);

            if (sensor_profile_notify_data(s_active_conn_handle, (const uint8_t *)payload, len) != ESP_OK)
            {
                ESP_LOGW(TAG, "Connection lost silently, stop notifying.");
                s_active_conn_handle = 0xFFFF;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Init
void ble_manager_init(void)
{
    // Nim port init
    ESP_LOGI(TAG, "Initializing NimBLE...");
    nimble_port_init();

    // Init NimBLE host cfg
    ble_hs_cfg.reset_cb = ble_hs_cfg_on_reset; //  void ble_on_reset
    ble_hs_cfg.sync_cb = ble_hs_cfg_on_sync;   //  void ble_on_sync(void)
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // ble_hs_cfg.gatts_register_cb = NULL;
    // ble_hs_cfg.store_status_cb = NULL;

    ble_svc_gap_device_name_set(BLE_DEVICE_NAME);

    ble_svc_gap_init();
    ble_svc_gatt_init();

    // register profiles

    // Provisioning
    ESP_ERROR_CHECK(prov_profile_init());
    prov_profile_register_cb(prov_profile_callback);
    // OTA
    ESP_ERROR_CHECK(ota_profile_init());
    ota_profile_register_cb(ota_profile_callback);

    // Tạo Queue (30 slot đủ đệm cho các gói tin BLE trôi chảy)
    s_ble_ota_queue = xQueueCreate(30, sizeof(ble_ota_msg_t));
    if (s_ble_ota_queue != NULL)
    {
        xTaskCreate(ble_ota_worker_task, "ble_ota_task", 4096, NULL, 5, &s_ble_ota_task_handle);
    }
    
    // SENSORS
    ESP_ERROR_CHECK(sensor_profile_init());
    sensor_profile_register_cb(sensor_profile_callback);

    // freeRTOS for ble is running in background
    nimble_port_freertos_init(ble_host_task);

    // notify task
    xTaskCreate(ble_notify_task, "ble_notify", 4096, NULL, 5, NULL);
}

// Deinit
void ble_manager_deinit(void)
{
    ble_manager_stop_advertise();

    nimble_port_stop();
    nimble_port_deinit();

    if (s_ble_ota_task_handle)
    {
        vTaskDelete(s_ble_ota_task_handle);
        s_ble_ota_task_handle = NULL;
    }

    if (s_ble_ota_queue)
    {
        vQueueDelete(s_ble_ota_queue);
        s_ble_ota_queue = NULL;
    }

    ESP_LOGI(TAG, "BLE service deinitialized");
}