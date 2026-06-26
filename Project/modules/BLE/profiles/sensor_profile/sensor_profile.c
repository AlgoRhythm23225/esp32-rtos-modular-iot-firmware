#include "sensor_profile.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_err.h"
#include "host/ble_hs.h"
#include "sensor_types.h"
#include "ble_types.h"
#include "sensors_manager.h"

static const char *TAG = "SENSOR_PROF";

// Sensor Service UUID: f3b890a2-f85f-4a00-b6f4-1ea15a9bc83d
static const ble_uuid128_t sensor_svc_uuid = BLE_UUID128_INIT(
    0x3d, 0xc8, 0x9b, 0x5a, 0xa1, 0x1e, 0xf4, 0xb6,
    0x00, 0x4a, 0x5f, 0xf8, 0xa2, 0x90, 0xb8, 0xf3);

// Sensor Data Characteristic UUID: 6a1b2c3d-4e5f-6a7b-8c9d-0e1f2a3b4c5d
static const ble_uuid128_t sensor_data_uuid = BLE_UUID128_INIT(
    0x5d, 0x4c, 0x3b, 0x2a, 0x1f, 0x0e, 0x9d, 0x8c,
    0x7b, 0x6a, 0x5f, 0x4e, 0x3d, 0x2c, 0x1b, 0x6a);

// Sensor Config Characteristic UUID: b1c2d3e4-f5a6-b7c8-d9e0-f1a2b3c4d5e6
static const ble_uuid128_t sensor_config_uuid = BLE_UUID128_INIT(
    0xe6, 0xd5, 0xc4, 0xb3, 0xa2, 0xf1, 0xe0, 0xd9,
    0xc8, 0xb7, 0xa6, 0xf5, 0xe4, 0xd3, 0xc2, 0xb1);

// a1b2c3d4-e5f6-1728-394a-5b6c7d8e9f10
static const ble_uuid128_t sensor_ctrl_uuid = BLE_UUID128_INIT(
    0x10, 0x9f, 0x8e, 0x7d, 0x6c, 0x5b, 0x4a, 0x39,
    0x28, 0x17, 0xf6, 0xe5, 0xd4, 0xc3, 0xb2, 0xa1);

static uint16_t data_val_handle;
static uint16_t config_val_handle;
static uint16_t control_val_handle;

static sensor_profile_cb_t app_cb = NULL;

static int handle_sensor_data_read(uint16_t conn_handle, struct ble_gatt_access_ctxt *ctxt)
{
    if (app_cb)
    {
        uint8_t buf[128] = {0};
        sensor_profile_cb_param_t param = {
            .event = SENSOR_PROFILE_EVT_READ_DATA,
            .conn_handle = conn_handle,
            .data = buf,
            .len = 0,
            .max_len = sizeof(buf)};

        int rc = app_cb(&param);
        if (rc == 0 && param.len > 0)
        {
            return os_mbuf_append(ctxt->om, param.data, param.len);
        }
        return rc == 0 ? BLE_ATT_ERR_UNLIKELY : rc;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static int handle_sensor_config_read(uint16_t conn_handle, struct ble_gatt_access_ctxt *ctxt)
{
    if (app_cb)
    {
        uint8_t buf[128] = {0};
        sensor_profile_cb_param_t param = {
            .event = SENSOR_PROFILE_EVT_READ_CONFIG,
            .conn_handle = conn_handle,
            .data = buf,
            .len = 0,
            .max_len = sizeof(buf)};

        int rc = app_cb(&param);
        if (rc == 0 && param.len > 0)
        {
            return os_mbuf_append(ctxt->om, param.data, param.len);
        }
        return rc == 0 ? BLE_ATT_ERR_UNLIKELY : rc;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static int handle_sensor_config_write(uint16_t conn_handle, struct ble_gatt_access_ctxt *ctxt)
{
    if (app_cb)
    {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        if (om_len > 128)
        {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        uint8_t buf[128] = {0};
        int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &om_len);
        if (rc != 0)
        {
            return BLE_ATT_ERR_UNLIKELY;
        }

        sensor_profile_cb_param_t param = {
            .event = SENSOR_PROFILE_EVT_WRITE_CONFIG,
            .conn_handle = conn_handle,
            .data = buf,
            .len = om_len,
            .max_len = sizeof(buf)};

        return app_cb(&param);
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// For sensor control ===============================================================================================
ble_sensor_state sensor_state_ble[SENSOR_MAX] = {
    {SENSOR_AHT20, false},
    {SENSOR_BH1750, false},
    {SENSOR_BMP280, false},
    {SENSOR_BNO055, false},
    {SENSOR_INA226, false},
};

// Implement for cmd from ble app
void ble_sensor_cmd_conn(uint16_t conn_handle, uint16_t attr_handle, ble_cmd_recv_t sensor_id) {
    esp_err_t ret;
    if (sensor_state_ble[sensor_id].is_connected == false) {
        if ((ret = sensor_init_table[sensor_id]()) == ESP_OK) {
            sensor_state_ble[sensor_id].is_connected = true;
            sensor_state[sensor_id].sensor_init_flag = true;
        }
    }
    else {
        ret = sensor_deinit_table[sensor_id]();
        sensor_state_ble[sensor_id].is_connected = false;
        sensor_state[sensor_id].sensor_init_flag = false;
    }
    uint8_t notify_data[2];
    notify_data[0] = sensor_id;
    notify_data[1] = (ret == ESP_OK && sensor_state_ble[sensor_id].is_connected == true) ? 0x00 : 0xFF;

    // update status
    struct os_mbuf *om = ble_hs_mbuf_from_flat(notify_data, sizeof(notify_data));
    if (om) {
        ble_gatts_notify_custom(conn_handle, attr_handle, om);
    }
}

// Implement for connect all cmd from ble app 
void ble_sensor_cmd_conn_all(uint16_t conn_handle, uint16_t attr_handle, bool connect_all) {
    esp_err_t ret = ESP_OK;

    for (int sensor_id = 0; sensor_id < SENSOR_MAX; sensor_id++) {
        // If it's Connect all command, init everything
        if (connect_all) {
            if (sensor_state_ble[sensor_id].is_connected == false) {
                ret = sensor_init_table[sensor_id]();
                sensor_state_ble[sensor_id].is_connected = true;
            }
        }
        // If it's disconnect all command, deinit everything
        else {
            if (sensor_state_ble[sensor_id].is_connected == true) {
            ret = sensor_deinit_table[sensor_id]();
            sensor_state_ble[sensor_id].is_connected = false;
            }
        }

        uint8_t notify_data[2];
        notify_data[0] = sensor_id;
        notify_data[1] = (ret == ESP_OK && sensor_state_ble[sensor_id].is_connected == true) ? 0x00 : 0xFF;

        // update status
        struct os_mbuf *om = ble_hs_mbuf_from_flat(notify_data, sizeof(notify_data));
        if (om) {
            ble_gatts_notify_custom(conn_handle, attr_handle, om);
        }  

        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

#define SEN_DELAY 3000
void ble_notify_sensor_data(ble_cmd_recv_t notify_cmd, uint8_t data_len, task_param_t param, float* data, uint8_t data_size) {
    uint8_t notify_data[data_len];
    notify_data[0] = notify_cmd;
    notify_data[1] = notify_cmd;

    memcpy(&notify_data[2], data, data_size);

    struct os_mbuf *om = ble_hs_mbuf_from_flat(notify_data, sizeof(notify_data));
    if (om) {
        ble_gatts_notify_custom(param.conn_handle, param.attr_handle, om);
    }  
}

// Enable sensor continous read
void ble_sensor_read(uint16_t conn_handle, uint16_t attr_handle, ble_cmd_recv_t data) {
    // Sensor ID
    uint8_t sensor_id = data - SENSOR_MAX;
    // task_param_t *param = malloc(sizeof(task_param_t));
    g_param.attr_handle = attr_handle;
    g_param.conn_handle = conn_handle;

    if (data >= AHT20_READ && data < SENSOR_MAX_READ) {
        // Read
        if (!is_reading[data - SENSOR_MAX]) {
            is_reading[sensor_id] = true;            
        }
        // Stop
        else {
            is_reading[sensor_id] = false;
        }

        // Packet to notify
        uint8_t notify_data[2];
        notify_data[0] = sensor_id;
        notify_data[1] = (is_reading[sensor_id] == true) ? 0x01 : 0x02;

        // update status
        struct os_mbuf *om = ble_hs_mbuf_from_flat(notify_data, sizeof(notify_data));
        if (om) {
            ble_gatts_notify_custom(conn_handle, attr_handle, om);
        }  
    }
}

void mqtt_publish_on(uint16_t conn_handle, uint16_t attr_handle, ble_cmd_recv_t data) {
    if (data == MQTT_PUBLISH) {
        if (!is_mqtt_on) {
            is_mqtt_on = true;
        }
        else {
            is_mqtt_on = false;
        }
        
        uint8_t notify_data[2];
        notify_data[0] = data;
        notify_data[1] = (is_mqtt_on == true) ? 0x03 : 0x04;
        
        // update status
        struct os_mbuf *om = ble_hs_mbuf_from_flat(notify_data, sizeof(notify_data));
        if (om) {
            ble_gatts_notify_custom(conn_handle, attr_handle, om);
        }
    }
}

bool is_thread_created = false;
static int handle_sensor_ctrl_write(uint16_t attr_handle, uint16_t conn_handle, struct ble_gatt_access_ctxt *ctxt) {
    uint8_t data = ctxt->om->om_data[0];
    ESP_LOGI(TAG, "Command received: \"%d\"", data);

    if (data < SENSOR_MAX) {
        if(!is_thread_created) {
            is_thread_created = true;
            notify_sensor_fn = ble_notify_sensor_data;

            xTaskCreate(
                sensor_task, 
                "sensor_task", 
                4096, 
                NULL, 
                5, 
                NULL
            );
        }
        ble_sensor_cmd_conn(conn_handle, attr_handle, data);
    }

    switch(data) {
        case ALL_CONNECT:
            if(!is_thread_created) {
                notify_sensor_fn = ble_notify_sensor_data;
                    is_thread_created = true;
                    
                xTaskCreate(
                    sensor_task, 
                    "sensor_task", 
                    4096, 
                    NULL, 
                    5, 
                    NULL
                );
            }
            ble_sensor_cmd_conn_all(conn_handle, attr_handle, true);
            break;
        case ALL_DISCONNECT:
            ble_sensor_cmd_conn_all(conn_handle, attr_handle, false);
            break;
        default:
            break;
    }

    // Enable sensor continous read    
    ble_sensor_read(conn_handle, attr_handle, data);

    // Enable mqtt publish
    mqtt_publish_on(conn_handle, attr_handle, data);

    return 0;
}
// For sensor control ===============================================================================================

// GATT access callback
static int sensor_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    ESP_LOGD(TAG, "Access CB: attr_handle=%d, op=%d", attr_handle, ctxt->op);

    if (attr_handle == data_val_handle)
    {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
        {
            return handle_sensor_data_read(conn_handle, ctxt);
        }
    }
    else if (attr_handle == config_val_handle)
    {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
        {
            return handle_sensor_config_read(conn_handle, ctxt);
        }
        else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
        {
            return handle_sensor_config_write(conn_handle, ctxt);
        }
    }
    else if (attr_handle == control_val_handle) 
    {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
        {
            return handle_sensor_ctrl_write(attr_handle, conn_handle, ctxt);
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

// GATT service
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &sensor_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                // Sensor Data Characteristic
                .uuid = &sensor_data_uuid.u,
                .access_cb = sensor_access_cb,
                .val_handle = &data_val_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            {
                // Sensor Config Characteristic
                .uuid = &sensor_config_uuid.u,
                .access_cb = sensor_access_cb,
                .val_handle = &config_val_handle,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
            },
            {
                // Sensor Control Characteristic
                .uuid = &sensor_ctrl_uuid.u,
                .access_cb = sensor_access_cb,
                .val_handle = &control_val_handle,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
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

esp_err_t sensor_profile_init(void)
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

    ESP_LOGI(TAG, "sensor profile initialized");
    return ESP_OK;
}

esp_err_t sensor_profile_register_cb(sensor_profile_cb_t cb)
{
    app_cb = cb;
    return ESP_OK;
}

esp_err_t sensor_profile_notify_data(uint16_t conn_handle, const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate mbuf for notification");
        return ESP_ERR_NO_MEM;
    }

    int rc = ble_gatts_notify_custom(conn_handle, data_val_handle, om);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to notify sensor data, rc=%d", rc);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Sensor data notified, len=%d", len);
    return ESP_OK;
}
