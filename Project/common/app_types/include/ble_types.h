#ifndef BLE_TYPES_h
#define BLE_TYPES_h

#include <stdint.h>

typedef enum {
    AHT20_CONNECT,
    BH1750_CONNECT,
    BMP280_CONNECT,
    BNO055_CONNECT,
    INA226_CONNECT,

    AHT20_READ,
    BH1750_READ,
    BMP280_READ,
    BNO055_READ,
    INA226_READ,
    SENSOR_MAX_READ, // Total number of read cmd

    AHT20_NOTIFY,
    BH1750_NOTIFY,
    BMP280_NOTIFY,
    BNO055_NOTIFY,
    INA226_NOTIFY,
    SENSOR_MAX_NOTIFY, // Total number of notify cmd

    SET_SAMPLE_RATE = 127,
    ALL_CONNECT = 128,
    ALL_DISCONNECT = 129,
    MQTT_PUBLISH = 130,
    ALL_READ = 131,
    ALL_STOP_READ = 132,
} ble_cmd_recv_t;

typedef struct {
    uint8_t sensor_id;
    bool is_connected;
} ble_sensor_state;

typedef struct {
    uint16_t conn_handle;
    uint16_t attr_handle;
} task_param_t;

extern task_param_t g_param;

typedef struct {
    ble_cmd_recv_t notify_cmd;
    uint8_t data_len;
    task_param_t* param;
    float* data;
    uint8_t data_size;
} ble_task_param_t;

typedef void (*ble_notify_fn_t)(ble_cmd_recv_t notify_cmd, uint8_t data_len, task_param_t param, float* data, uint8_t data_type_len);
extern ble_notify_fn_t notify_sensor_fn; 
#endif