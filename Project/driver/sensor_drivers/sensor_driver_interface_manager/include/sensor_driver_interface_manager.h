#ifndef SENSOR_INTERFACE_MANAGER_H
#define SENSOR_INTERFACE_MANAGER_H

#include "i2c_manager.h"
#include "sensor_types.h"
#include "sensor_error.h"

uint8_t sensor_driver_interface_init(i2c_master_dev_handle_t *dev_handle, sensor_id_t sensor_id);
uint8_t sensor_driver_interface_deinit(i2c_master_dev_handle_t *dev_handle, sensor_id_t sensor_id);

#endif