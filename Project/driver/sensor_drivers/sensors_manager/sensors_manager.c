#include "sensors_manager.h"
#include "i2c_error.h"
#include "ble_types.h"
#include "sensor_types.h"
#include "rtos_config.h"

#define AWS_DEVICE_ID "esp32-001"
#define TOPIC_CMD     "devices/" AWS_DEVICE_ID "/ota/command"
#define TOPIC_OTA_CMD "devices/" AWS_DEVICE_ID "/ota/status"

static const char* TAG = "sensor_manager";
static uint8_t error_trigger_flag[SENSOR_MAX];

// For reconnecting when sensor connection is lost
typedef uint8_t (*sensor_i2c_bus_init_fn_t)(void);
sensor_i2c_bus_init_fn_t sensor_i2c_bus_init_table[SENSOR_MAX] = {
    aht20_interface_iic_init,
    bh1750fvi_interface_iic_init,
    bmp280_interface_iic_init,
    bno055_interface_iic_init,
    ina226_interface_iic_init,
};

// For reconnecting when everything is broken
sensor_init_fn_t sensor_init_table[SENSOR_MAX] = {
    aht20_full_init,
    bh1750fvi_full_init,
    bmp280_full_init,
    bno055_full_init,
    ina226_full_init,
};

// For remove bus
sensor_deinit_fn_t sensor_deinit_table[SENSOR_MAX] = {
    aht20_interface_iic_deinit,
    bh1750fvi_interface_iic_deinit,
    bmp280_interface_iic_deinit,
    bno055_interface_iic_deinit,
    ina226_interface_iic_deinit,
};

/**
 * @brief Packing Json for publishing to Broker
 * @param[out]  buffer packed data buffer 
 * @param[in]   buffer_size raw data size 
 * @param[in]   humidity %
 * @param[in]   lux lux
 * @param[in]   temperature Celcius 
 * @param[in]   pressure Pa
 * @param[in]   accel 
 * @param[in]   battery %
 */
static void sensor_json(char *buffer, uint8_t buffer_size, sensor_data_t sensor_data) {
    snprintf(buffer, buffer_size,
                "{"
                "\"humidity\":%u,"
                "\"lux\":%.1f,"
                "\"temperature\":%.1f,"
                "\"pressure\":%.2f,"
                "\"accel\":%.2f,"
                "\"battery\":%.1f"                
                "}",
            sensor_data.humidity, sensor_data.lux, 
            sensor_data.temperature, sensor_data.pressure, 
            sensor_data.accel, sensor_data.battery);
}

// Initialize all sensors
void sensor_init() {
    for (int i = 0; i < SENSOR_MAX; i++) {
        if (sensor_init_table[i]() == ESP_OK) {
            sensor_state[i].sensor_init_flag = true;
        }
        else {
            sensor_state[i].sensor_init_flag = false;
        }
    }

}

// if the error is small, reset bus, if everything is broken, reset all
static void sensor_eror_check_and_set_flag(uint8_t dev_addr, sensor_id_t dev_id) {
    esp_err_t ret;
    if ((ret = i2c_master_probe(i2c_get_bus_handle(), dev_addr, 100)) != ESP_OK) {
        ESP_LOGE(TAG, "0x%02X got bus error, trying to fix...", dev_addr);
        sensor_state[dev_id].i2c_init_flag = false;
        error_trigger_flag[dev_id] = SENSOR_BUS_ERR;
    }
    else {
        ESP_LOGE(TAG, "0x%02X GOT hardware error, trying to fix...", dev_addr);
        sensor_state[dev_id].sensor_init_flag = false;
        error_trigger_flag[dev_id] = SENSOR_HARDWARE_ERR;
    }
}

// Find and fix which sensor cause the bus error
static void sensor_find_error_and_fix_small() {
    for (int i = 0; i < SENSOR_MAX; i++) {
        if (sensor_state[i].i2c_init_flag == false) {
            sensor_deinit_table[i]();
            sensor_i2c_bus_init_table[i]();
            error_trigger_flag[i] = SENSOR_OK;
        }
    }
}

// Find and fix which sensor cause the big error
static void sensor_find_error_and_fix_all() {
    for (int i = 0; i < SENSOR_MAX; i++) {
        if (sensor_state[i].sensor_init_flag == false) {
            sensor_deinit_table[i]();
            sensor_init_table[i]();
            error_trigger_flag[i] = SENSOR_OK;
        }
    }
}

// Sample Rate
static bool can_read_now(TickType_t now, sensor_id_t sensor_id) {
    return (now - sensor_timer[sensor_id].last_tick) >= sensor_timer[sensor_id].period;
}

// Read data
static sensor_data_t sensor_read_all() {
    TickType_t now = xTaskGetTickCount();
    sensor_data_t sensor_data = {0};

    // Humidity
    if (is_reading[SENSOR_AHT20] && can_read_now(now, SENSOR_AHT20)) {
        sensor_timer[SENSOR_AHT20].last_tick = now;
        if (aht20_app_read_hum(&sensor_data.humidity) != ESP_OK) {
            sensor_data.humidity = 0;
            sensor_eror_check_and_set_flag(AHT20_ADDRESS, SENSOR_AHT20);
        }
        notify_sensor_fn(AHT20_NOTIFY, 6, g_param, (float*)&sensor_data.humidity, sizeof(uint8_t));
    }

    // Lux
    if (is_reading[SENSOR_BH1750] && can_read_now(now, SENSOR_BH1750)) {
        sensor_timer[SENSOR_BH1750].last_tick = now;
        if (bh1750fvi_basic_read(&sensor_data.lux) != ESP_OK) {
            sensor_data.lux = 0;
            sensor_eror_check_and_set_flag(BH1750FVI_ADDRESS, SENSOR_BH1750);
        }
        notify_sensor_fn(BH1750_NOTIFY, 6, g_param, &sensor_data.lux, sizeof(float));
    }
    
    // Temperature + pressure
    if (is_reading[SENSOR_BMP280] && can_read_now(now, SENSOR_BMP280)) {
        sensor_timer[SENSOR_BMP280].last_tick = now;
        if (bmp280_app_read(&sensor_data.temperature, &sensor_data.pressure) != ESP_OK) {
            sensor_data.temperature = 0;
            sensor_data.pressure = 0;
            sensor_eror_check_and_set_flag(BMP280_ADDRESS, SENSOR_BMP280);
        }
        float data[2];
        data[0] = sensor_data.pressure;
        data[1] = sensor_data.temperature;
        notify_sensor_fn(BMP280_NOTIFY, 10, g_param, data, sizeof(data));
    }

    // Accelarate
    if (is_reading[SENSOR_BNO055] && can_read_now(now, SENSOR_BNO055)) {
        sensor_timer[SENSOR_BNO055].last_tick = now;
        if (bno055_accel_read(&sensor_data.accel) != ESP_OK) {
            sensor_data.accel = 0;
            sensor_eror_check_and_set_flag(BNO055_ADDRESS, SENSOR_BNO055);
        }
        notify_sensor_fn(BNO055_NOTIFY, 6, g_param, &sensor_data.accel, sizeof(float));
    }

    // Battery
    if (is_reading[SENSOR_INA226] && can_read_now(now, SENSOR_INA226)) {
        sensor_timer[SENSOR_INA226].last_tick = now;
        if (ina226_read_get_battery(&sensor_data.battery) != ESP_OK) {
            sensor_data.battery = 0;
            sensor_eror_check_and_set_flag(INA226_ADDRESS, SENSOR_INA226);
        }
        notify_sensor_fn(INA226_NOTIFY, 6, g_param, &sensor_data.battery, sizeof(float));
    }

    return sensor_data;
}

// Read and get all data and converse to JSON
void sensor_get_data_json_packet(char *buffer, int buffer_size) {
    sensor_data_t sensor_data;
    
    // Get data
    sensor_data = sensor_read_all();
    // Pack JSON
    sensor_json(buffer, buffer_size, sensor_data);
}

void sensor_task(void *pvParameter) {
    // For sure
    i2c_init();

    // Init Sample rate    
    sensor_timer[SENSOR_AHT20].period = pdMS_TO_TICKS(1000);
    sensor_timer[SENSOR_BH1750].period = pdMS_TO_TICKS(1000);
    sensor_timer[SENSOR_BMP280].period = pdMS_TO_TICKS(1000);
    sensor_timer[SENSOR_BNO055].period = pdMS_TO_TICKS(500);
    sensor_timer[SENSOR_INA226].period = pdMS_TO_TICKS(30000);

    // Init Flag
    for (int i = 0; i < SENSOR_MAX; i++) {
        error_trigger_flag[i] = SENSOR_OK;
    }

    // Buffer for publish or notify
    char buffer[128];
    memset(buffer, 0, sizeof(buffer));

    // Message for publish
    mqtt_publish_msg_t msg;

    while (1) {
        for (int i = 0; i < SENSOR_MAX; i++) {
            if(error_trigger_flag[i] != SENSOR_OK) {
                switch (error_trigger_flag[i]) {
                    case SENSOR_BUS_ERR:
                        sensor_find_error_and_fix_small();
                        break;
                    case SENSOR_HARDWARE_ERR:
                        sensor_find_error_and_fix_all();
                        break;
                    default:
                        // Unknown error trigger flag
                        break;
                }
            }
        }

        // packaging JSON data
        sensor_get_data_json_packet(buffer, sizeof(buffer));
        
        if (is_mqtt_on) {
            // add topic
            strncpy(msg.topic, TOPIC_CMD, sizeof(msg.topic) - 1);
            msg.topic[sizeof(msg.topic) - 1] = '\0';
            // add payload from buffer
            strncpy(msg.payload, buffer, sizeof(msg.payload) - 1);
            msg.payload[sizeof(msg.payload) - 1] = '\0';
            // add payload length
            msg.payload_len = strlen(msg.payload);
            printf("%s\n", buffer);
            
            // send to publish queue
            if (xQueueSend(g_mqtt_publish_queue, &msg, 0) != pdTRUE) {
                ESP_LOGE("Sensor_task", "Publish queue full - data droped !");
            }
        }

        // // send to notify queue
        // if (xQueueSend(g_ble_send_queue, &msg, 0) != pdTRUE) {
        //     ESP_LOGE("Sensor_task", "BLE queue full - data droped !");
        // }  
        vTaskDelay(pdMS_TO_TICKS(10));  
    }
}

