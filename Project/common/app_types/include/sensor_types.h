#ifndef SENSOR_TYPES_H
#define SENSOR_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"

#define AHT20_ADDRESS       0x38
#define BH1750FVI_ADDRESS   0x23
#define BMP280_ADDRESS      0x76
#define BNO055_ADDRESS      0x29
#define INA226_ADDRESS      0x40

typedef struct {
    char *device;
    uint8_t addr;
    bool i2c_init_flag;
    bool sensor_init_flag;
}sensor_state_t;

typedef enum {
    SENSOR_AHT20,
    SENSOR_BH1750,
    SENSOR_BMP280,
    SENSOR_BNO055,
    SENSOR_INA226,
    SENSOR_MAX
} sensor_id_t;

typedef struct {
    uint8_t humidity;       // AHT20
    float lux;              // BH1750
    float temperature;      // BMP280
    float pressure;         // BMP280
    float accel;            // BNO055
    float battery;          // INA226
} sensor_data_t;

// State of reading
extern bool is_reading[SENSOR_MAX];

// State of connection and working properly
extern sensor_state_t sensor_state[SENSOR_MAX];

// Convert functions
uint8_t sensor_id_to_address(sensor_id_t sensor_id);
const char *sensor_id_to_str(sensor_id_t sensor_id);

// State of MQTT publish
extern bool is_mqtt_on;

typedef struct {
    TickType_t last_tick;
    TickType_t period;
} sensor_timer_t;
extern sensor_timer_t sensor_timer[SENSOR_MAX];

#endif