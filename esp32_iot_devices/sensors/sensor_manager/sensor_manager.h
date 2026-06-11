#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "i2c_manager.h"

#define SENSOR_PROCESS_LOOP_MS    5000

#define AHT20_ADDRESS       0x38
#define BH1750FVI_ADDRESS   0x23
#define BMP280_ADDRESS      0x76
#define BNO055_ADDRESS      0x29
#define INA226_ADDRESS      0x40

typedef enum {
    SENSOR_AHT20,
    SENSOR_BH1750,
    SENSOR_BMP280,
    SENSOR_BNO055,
    SENSOR_INA226,
    SENSOR_MAX
} sensor_id_t;

extern sensor_state_t sensor_state[SENSOR_MAX];


void sensor_init();
void sensor_get_data_json_packet(char *buffer, int buffer_size);
void sensor_task(void *pvParameter);

#endif