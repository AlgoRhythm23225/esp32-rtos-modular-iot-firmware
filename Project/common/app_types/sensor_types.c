#include "sensor_types.h"

sensor_state_t sensor_state[SENSOR_MAX] = {
    {"aht20",  0, false, false},
    {"bh1750", 0, false, false},
    {"bmp280", 0, false, false},
    {"bno055", 0, false, false},
    {"ina226", 0, false, false},
};

/**
 * @brief Convert sensor ID to address
 * @param[in] sensor_id 
 */
uint8_t sensor_id_to_address(sensor_id_t sensor_id) {
    switch (sensor_id) {
        case SENSOR_AHT20:
            return AHT20_ADDRESS;
        case SENSOR_BH1750:
            return BH1750FVI_ADDRESS;
        case SENSOR_BMP280:
            return BMP280_ADDRESS;
        case SENSOR_BNO055:
            return BNO055_ADDRESS;
        case SENSOR_INA226:
            return INA226_ADDRESS;
        default:
            return 0x00;
    }
}

/**
 * @brief Convert sensor ID to string
 * @param[in] sensor_id 
 */
const char *sensor_id_to_str(sensor_id_t sensor_id) {
    switch (sensor_id) {
        case SENSOR_AHT20:  
            return "AHT20";
        case SENSOR_BH1750: 
            return "BH1750";
        case SENSOR_BMP280: 
            return "BMP280";
        case SENSOR_BNO055: 
            return "BNO055";
        case SENSOR_INA226: 
            return "INA226";
        default:
            return "UNKNOWN";
    }
}

bool is_reading[SENSOR_MAX] = {false};
bool is_mqtt_on = false;