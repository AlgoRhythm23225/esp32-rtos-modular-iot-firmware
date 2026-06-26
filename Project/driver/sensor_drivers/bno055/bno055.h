#include "freertos/FreeRTOS.h" // data types

#include "driver_bno055.h"
#include "driver_bno055_interface.h"

void bno055_ndof_task(void *pvParameters);
esp_err_t bno055_full_init();
uint8_t bno055_accel_read(float *accel);