#include "sensor_manager.h"
#include "aht20.h"
#include "bh1750.h"
#include "bmp280.h"
#include "bno055.h"
#include "ina226.h"
#include "i2c_error.h"

#define AWS_DEVICE_ID "esp32-001"
#define TOPIC_CMD     "devices/" AWS_DEVICE_ID "/ota/command"
#define TOPIC_OTA_CMD "devices/" AWS_DEVICE_ID "/ota/status"

typedef enum {
    SENSOR_NO_ERR,
    SENSOR_BUS_ERR,
    SENSOR_HARDWARE_ERR,
} sensor_err_t;

static const char* TAG = "sensor_manager";
static uint8_t error_trigger_flag = SENSOR_NO_ERR;

sensor_state_t sensor_state[SENSOR_MAX] = {
    {"aht20",  0, false, false},
    {"bh1750", 0, false, false},
    {"bmp280", 0, false, false},
    {"bno055", 0, false, false},
    {"ina226", 0, false, false},
};

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
typedef esp_err_t (*sensor_init_fn_t)(void);
sensor_init_fn_t sensor_init_table[SENSOR_MAX] = {
    aht20_full_init,
    bh1750fvi_full_init,
    bmp280_full_init,
    bno055_full_init,
    ina226_full_init,
};

/**
 * @brief Packing Json
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
    esp_err_t ret;

    for (int i = 0; i< SENSOR_MAX; i++) {
        if (sensor_init_table[i]() == ESP_OK) {
            sensor_state[i].sensor_init_flag = true;
        }
        else {
            sensor_state[i].sensor_init_flag = false;
        }
    }
}

// if the error is small, reset bus, if everything is broken, reset all
void sensor_eror_check_and_set_flag(uint8_t dev_addr, sensor_id_t dev_id) {
    esp_err_t ret;
    if ((ret = i2c_master_probe(i2c_get_bus_handle(), dev_addr, 100)) != ESP_OK) {
        ESP_LOGE(TAG, "0x%02X got bus error, trying to fix...", dev_addr);
        sensor_state[dev_id].i2c_init_flag = false;
        error_trigger_flag = SENSOR_BUS_ERR;
    }
    else {
        ESP_LOGE(TAG, "0x%02X GOT hardware error, trying to fix...", dev_addr);
        sensor_state[dev_id].sensor_init_flag = false;
        error_trigger_flag = SENSOR_HARDWARE_ERR;
    }
}

// Find and fix which sensor cause the bus error
void sensor_find_error_and_fix_small() {
    for (int i = 0; i < SENSOR_MAX; i++) {
        if (sensor_state[i].i2c_init_flag == false) {
            sensor_i2c_bus_init_table[i]();
            error_trigger_flag = SENSOR_NO_ERR;
        }
    }
}

// Find and fix which sensor cause the big error
void sensor_find_error_and_fix_all() {
    for (int i = 0; i < SENSOR_MAX; i++) {
        if (sensor_state[i].sensor_init_flag == false) {
            sensor_init_table[i]();
            error_trigger_flag = SENSOR_NO_ERR;
        }
    }
}

// Read data
static sensor_data_t sensor_read_all() {
    sensor_data_t sensor_data = {0};
    // Humidity
    if (aht20_app_read_hum(&sensor_data.humidity) != ESP_OK) {
        sensor_data.humidity = 0;
        sensor_eror_check_and_set_flag(AHT20_ADDRESS, SENSOR_AHT20);
    }

    // Lux
    if (bh1750fvi_basic_read(&sensor_data.lux) != ESP_OK) {
        sensor_data.lux = 0;
        sensor_eror_check_and_set_flag(BH1750FVI_ADDRESS, SENSOR_BH1750);
    }
    
    // Temperature + pressure
    if (bmp280_app_read(&sensor_data.temperature, &sensor_data.pressure) != ESP_OK) {
        sensor_data.temperature = 0;
        sensor_data.pressure = 0;
        sensor_eror_check_and_set_flag(BMP280_ADDRESS, SENSOR_BMP280);
    }

    // Accelarate
    if (bno055_accel_read(&sensor_data.accel) != ESP_OK) {
        sensor_data.accel = 0;
        sensor_eror_check_and_set_flag(BNO055_ADDRESS, SENSOR_BNO055);
    }

    // Battery
    if (ina226_read_get_battery(&sensor_data.battery) != ESP_OK) {
        sensor_data.battery = 0;
        sensor_eror_check_and_set_flag(INA226_ADDRESS, SENSOR_INA226);
    }

    return sensor_data;
}

// Read and get all data and converse to JSON
void sensor_get_data_json_packet(char *buffer, int buffer_size) {
    sensor_data_t sensor_data;
    
    sensor_data = sensor_read_all();
    sensor_json(buffer, buffer_size, sensor_data);
}

void sensor_task(void *pvParameter) {
    i2c_init();
    sensor_init();

    char buffer[128];
    memset(buffer, 0, sizeof(buffer));

    mqtt_publish_msg_t msg;

    while (1) {
        switch (error_trigger_flag) {
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

        sensor_get_data_json_packet(buffer, sizeof(buffer));
        printf("%s\n", buffer);

        // Package data for publish
        strncpy(msg.topic, TOPIC_CMD, sizeof(msg.topic) - 1);
        msg.topic[sizeof(msg.topic) - 1] = '\0';    
        strncpy(msg.payload, buffer, sizeof(msg.payload) - 1);
        msg.payload[sizeof(msg.payload) - 1] = '\0';
        msg.payload_len = strlen(msg.payload);

        if (xQueueSend(g_mqtt_publish_queue, &msg, 0) != pdTRUE) {
            ESP_LOGE("Sensor_task", "Publish queue full - data droped !");
        }
        if (xQueueSend(g_ble_send_queue, &msg, 0) != pdTRUE) {
            ESP_LOGE("Sensor_task", "BLE queue full - data droped !");
        }
        vTaskDelay(pdMS_TO_TICKS(SENSOR_PROCESS_LOOP_MS));
    }
}