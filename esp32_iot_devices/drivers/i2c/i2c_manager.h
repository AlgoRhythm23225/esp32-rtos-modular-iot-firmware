#include "driver/i2c_master.h"

#define I2C_MASTER_SCL_IO   18
#define I2C_MASTER_SDA_IO   19
#define I2C_MASTER_NUM      I2C_NUM_1  

extern const char *TAG_I2C;

void i2c_init();
void i2c_add_device(uint8_t dev_addr, i2c_master_dev_handle_t *dev_handle);
esp_err_t i2c_write_sensor(i2c_master_dev_handle_t dev_handle, uint8_t *buf, uint16_t len);
esp_err_t i2c_read_sensor(i2c_master_dev_handle_t dev_handle, uint8_t *buf, uint16_t len);
esp_err_t i2c_write_read_sensor(i2c_master_dev_handle_t dev_handle, 
                            uint8_t *buf_write, size_t write_len, 
                            uint8_t *buf_read, uint16_t len);