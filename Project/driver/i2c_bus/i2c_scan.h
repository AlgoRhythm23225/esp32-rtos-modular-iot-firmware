#include "driver/i2c_master.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO_SCAN   18
#define I2C_MASTER_SDA_IO_SCAN   19
#define I2C_MASTER_NUM_SCAN      I2C_NUM_1

void i2c_scan_init();
void i2c_scan();

