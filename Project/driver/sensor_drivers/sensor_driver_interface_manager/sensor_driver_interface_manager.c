#include "sensor_driver_interface_manager.h"
#include "i2c_error.h"
#include "esp_log.h"

/**
 * @brief An interface for sensor initialization function
 */
uint8_t sensor_driver_interface_init(i2c_master_dev_handle_t *dev_handle, sensor_id_t sensor_id)
{
    if (!dev_handle) {
        ESP_LOGW(TAG_I2C, "Pointer to %s handle is NULL!", sensor_id_to_str(sensor_id));
        return SENSOR_HANDLE_NULL;
    }

    if (*dev_handle != NULL) {
        ESP_LOGW(TAG_I2C, "%s is alreardy initialized!", sensor_id_to_str(sensor_id));
        return SENSOR_ALREADY_INITIALIZED;
    }

    sensor_state_t sensor_state_tmp;
    sensor_state_tmp = i2c_add_device(sensor_id_to_address(sensor_id), dev_handle);
    sensor_state[sensor_id].addr = sensor_state_tmp.addr;
    sensor_state[sensor_id].i2c_init_flag = sensor_state_tmp.i2c_init_flag;
    
    if (!sensor_state[sensor_id].i2c_init_flag) {
        return SENSOR_BUS_ERR;
    }

    ESP_LOGI(TAG_I2C, "%s sensor added to I2C bus!", sensor_id_to_str(sensor_id));

    return SENSOR_OK;
}

/**
 * @brief An interface for sensor de-initialization function
 */
uint8_t sensor_driver_interface_deinit(i2c_master_dev_handle_t *dev_handle, sensor_id_t sensor_id)
{
    esp_err_t ret;

    if (!dev_handle) {
        ESP_LOGW(TAG_I2C, "%s device hanlde is NULL!", sensor_id_to_str(sensor_id));
        return SENSOR_HANDLE_NULL;
    }

    if (*dev_handle == NULL) {
        ESP_LOGW(TAG_I2C, "%s device handle is ALREADY NULL!", sensor_id_to_str(sensor_id));
        return SENSOR_OK;
    }

    ESP_LOGI(TAG_I2C, "Removing %s...", sensor_id_to_str(sensor_id));

    if ((ret = i2c_master_bus_rm_device(*dev_handle)) != ESP_OK) {
        ESP_LOGE(TAG_I2C, "Failed to remove AHT20 device: %s", esp_err_to_name(ret));
        return SENSOR_DEINIT_FAIL;
    }

    *dev_handle = NULL;  
    sensor_state[sensor_id].sensor_init_flag = false;
    ESP_LOGI(TAG_I2C, "%s sensor removed from I2C bus!", sensor_id_to_str(sensor_id));
    
    return SENSOR_OK;
}