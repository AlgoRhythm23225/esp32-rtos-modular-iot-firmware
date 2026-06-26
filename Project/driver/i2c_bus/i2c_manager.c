#include "i2c_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

const char *TAG_I2C = "I2C";

static i2c_master_bus_handle_t bus_handle = NULL;
static bool is_i2c_inited = false;

void i2c_init() {
    if(is_i2c_inited) {
        return;
    }

    if (bus_handle != NULL) {
        return;
    }

    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus_handle));     
    is_i2c_inited = true;
}

static i2c_device_config_t i2c_get_dev_config(uint8_t dev_addr) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_7,
        .device_address = dev_addr,
        .scl_speed_hz = 100000,               // 100 khz
    };

    return dev_cfg;
}

sensor_state_t i2c_add_device(uint8_t dev_addr, i2c_master_dev_handle_t *dev_handle) {
    esp_err_t ret;
    sensor_state_t sensor_state = {0};
    // Error handling: no device response, system keeps retrying until success
    if ((ret = i2c_master_probe(bus_handle, dev_addr, 100)) != ESP_OK) {
        ESP_LOGE(TAG_I2C, "%s Can't find address 0x%02X, please check the wiring...", i2c_addr_to_name(dev_addr) ,dev_addr);
        sensor_state.i2c_init_flag = false;
        sensor_state.addr = dev_addr;
        return sensor_state;
    }

    i2c_device_config_t dev_cfg = i2c_get_dev_config(dev_addr);
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, dev_handle));
    sensor_state.i2c_init_flag = true;
    sensor_state.addr = dev_addr;

    return sensor_state;
}

esp_err_t i2c_write_sensor(i2c_master_dev_handle_t dev_handle, uint8_t* buf, uint16_t len) {
    esp_err_t ret;
    for (int i = 0; i < I2C_MAX_RETRY; i++) {
        ret = i2c_master_transmit(dev_handle, buf, len, 500);
        if (ret == ESP_OK) return ESP_OK;

        // Return if code wrong
        if (ret == ESP_ERR_INVALID_ARG) {
            ESP_LOGE(TAG_I2C, "Invalid arg, aborting!");
            return ret;
        }

        ESP_LOGW(TAG_I2C, "Write fail, code %d: %s ...", I2C_MAX_RETRY, esp_err_to_name(ret));
        ESP_LOGW(TAG_I2C, "(%d)Trying to write again in %dms ...", i + 1, I2C_RETRY_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(I2C_RETRY_DELAY_MS));
    }
    return ret;
}

esp_err_t i2c_read_sensor(i2c_master_dev_handle_t dev_handle, uint8_t* buf, uint16_t len) {
    esp_err_t ret;
    for (int i = 0; i < I2C_MAX_RETRY; i++) {
        ret = i2c_master_receive(dev_handle, buf, len, 500);
        if (ret == ESP_OK) return ESP_OK;

        // Return if code wrong
        if (ret == ESP_ERR_INVALID_ARG) {
            ESP_LOGE(TAG_I2C, "Invalid arg, aborting!");
            return ret;
        }

        ESP_LOGW(TAG_I2C, "Read fail, code %d: %s ...", I2C_MAX_RETRY, esp_err_to_name(ret));
        ESP_LOGW(TAG_I2C, "(%d)Trying to read again in %dms ...", i + 1, I2C_RETRY_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(I2C_RETRY_DELAY_MS));
    }
    return ret;
}

esp_err_t i2c_write_read_sensor(i2c_master_dev_handle_t dev_handle, 
                            uint8_t* buf_write, size_t write_len, 
                            uint8_t* buf_read, uint16_t len) 
{
    esp_err_t ret;
    for (int i = 0; i < I2C_MAX_RETRY; i++) {
        ret = i2c_master_transmit_receive(dev_handle, buf_write, write_len, buf_read, len, 500);
        if (ret == ESP_OK) return ESP_OK;

        // Return if code wrong
        if (ret == ESP_ERR_INVALID_ARG) {
            ESP_LOGE(TAG_I2C, "Invalid arg, aborting!");
            return ret;
        }

        ESP_LOGW(TAG_I2C, "Read/write fail, code %d: %s ...", I2C_MAX_RETRY, esp_err_to_name(ret));
        ESP_LOGW(TAG_I2C, "(%d)Trying to read/write again in %dms ...", i + 1, I2C_RETRY_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(I2C_RETRY_DELAY_MS));
    }
    return ret;
}

i2c_master_bus_handle_t i2c_get_bus_handle() {
    return bus_handle;
}